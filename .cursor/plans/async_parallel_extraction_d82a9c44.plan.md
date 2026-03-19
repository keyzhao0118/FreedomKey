---
name: Async Parallel Extraction
overview: Refactor the extraction pipeline to decouple decoding from disk IO using a producer-consumer async write buffer, and support multi-threaded parallel file extraction for non-solid archives (ZIP), achieving maximum decompression throughput.
todos:
  - id: write-buffer-queue
    content: Create WriteBufferQueue class - thread-safe bounded producer-consumer buffer with QMutex/QWaitCondition
    status: completed
  - id: async-out-stream
    content: Create AsyncOutStreamWrapper - ISequentialOutStream that enqueues to WriteBufferQueue instead of disk IO
    status: completed
  - id: file-writer-thread
    content: Create FileWriterThread - persistent QThread that drains WriteBufferQueue, writes to disk, and sets file metadata (timestamps/attributes)
    status: completed
  - id: extract-worker
    content: Create ExtractWorker QRunnable - opens its own IInArchive, runs Extract() for a subset of file indices with async IO
    status: completed
  - id: modify-extract-callback
    content: Modify ArchiveExtractCallBack to use AsyncOutStreamWrapper, pre-fetch metadata in GetStream, defer attribute-setting to FileWriterThread
    status: completed
  - id: refactor-extractor
    content: "Refactor ArchiveExtractor as orchestrator: detect solid/non-solid, partition indices, manage QThreadPool of ExtractWorkers, poll and aggregate progress"
    status: completed
  - id: update-window
    content: Update KeyZipWindow progress signal connection from BlockingQueuedConnection to QueuedConnection
    status: completed
  - id: update-cmake
    content: Add new source files to CMakeLists.txt
    status: completed
isProject: false
---

# Async Parallel Extraction Pipeline

## Problem Analysis

Current extraction flow in [archiveextractor.cpp](KeyZip/archiveextractor.cpp) is fully serial within a single thread:

```mermaid
sequenceDiagram
    participant SDK as 7z SDK
    participant Out as OutStreamWrapper
    participant Disk as QFile (Disk)

    loop For each file
        SDK->>Out: GetStream(index)
        Out->>Disk: Open file
        loop For each decoded chunk
            SDK->>Out: Write(data)
            Out->>Disk: QFile::write(data)
            Note over SDK: BLOCKED waiting for IO
            Disk-->>Out: done
            Out-->>SDK: done
        end
        SDK->>Out: SetOperationResult
    end
```



Two bottlenecks:

1. **Decode-IO coupling**: Every `OutStreamWrapper::Write()` call in [outstreamwrapper.cpp](KeyZip/outstreamwrapper.cpp) does synchronous `QFile::write()`, blocking the decoder until the OS completes the disk write.
2. **Single-threaded decode**: Only one `Extract()` call runs at a time, even for formats like ZIP where each file is independently compressed.

## Target Architecture

```mermaid
flowchart TB
    subgraph orchestrator [ArchiveExtractor - Orchestrator Thread]
        detect[Detect solid / non-solid]
        partition[Partition file indices]
        wait[Wait for all workers]
    end

    subgraph decodePool [Decode QThreadPool]
        W1[ExtractWorker 1<br/>Extract indices_1]
        W2[ExtractWorker 2<br/>Extract indices_2]
        WN[ExtractWorker N<br/>Extract indices_N]
    end

    subgraph ioLayer [IO Layer - per worker]
        BQ1[WriteBufferQueue 1]
        BQ2[WriteBufferQueue 2]
        BQN[WriteBufferQueue N]
        FW1[FileWriterThread 1]
        FW2[FileWriterThread 2]
        FWN[FileWriterThread N]
    end

    detect --> partition --> decodePool
    W1 -->|"enqueue (non-blocking)"| BQ1 --> FW1 -->|QFile::write| Disk[(Disk)]
    W2 -->|"enqueue (non-blocking)"| BQ2 --> FW2 -->|QFile::write| Disk
    WN -->|"enqueue (non-blocking)"| BQN --> FWN -->|QFile::write| Disk
    decodePool --> wait
```



**Key principle**: The decoder's `Write()` call copies data into an in-memory bounded buffer and returns immediately. A dedicated IO thread drains the buffer and writes to disk in parallel. Backpressure is applied when the buffer is full (decoder blocks on enqueue), preventing unbounded memory growth.

## Strategy by Archive Format


| Format         | Solid? | Decode Threads                               | Async IO |
| -------------- | ------ | -------------------------------------------- | -------- |
| ZIP            | No     | N workers, each handles a partition of files | Yes      |
| 7z (non-solid) | No     | N workers                                    | Yes      |
| 7z (solid)     | Yes    | 1 worker (SDK constraint)                    | Yes      |
| RAR            | Varies | 1 worker (conservative)                      | Yes      |


Detection: query `kpidSolid` property from the archive via `IInArchive::GetArchiveProperty()`.

## Component Design

### 1. WriteBufferQueue (new: `writebufferqueue.h/.cpp`)

Thread-safe bounded queue using `QMutex` + `QWaitCondition`.

```cpp
class WriteBufferQueue {
public:
    explicit WriteBufferQueue(qint64 maxBufferBytes = 4 * 1024 * 1024);
    void enqueue(const char* data, quint32 size);  // blocks if full
    QByteArray dequeue();                           // blocks if empty
    void markFinished();
    bool isFinished() const;
private:
    QQueue<QByteArray> m_queue;
    QMutex m_mutex;
    QWaitCondition m_notFull, m_notEmpty;
    qint64 m_currentBytes = 0;
    qint64 m_maxBytes;
    bool m_finished = false;
};
```

### 2. AsyncOutStreamWrapper (new: `asyncoutstreamwrapper.h/.cpp`)

Drop-in replacement for [OutStreamWrapper](KeyZip/outstreamwrapper.h), implements `ISequentialOutStream`. `Write()` enqueues data to `WriteBufferQueue` instead of doing disk IO.

```cpp
STDMETHODIMP AsyncOutStreamWrapper::Write(const void* data, UInt32 size, UInt32* processedSize) {
    m_bufferQueue->enqueue(static_cast<const char*>(data), size);
    if (processedSize) *processedSize = size;
    return S_OK;
}
```

### 3. FileWriterThread (new: `filewriterthread.h/.cpp`)

A persistent `QThread` that consumes data from `WriteBufferQueue` instances and writes to disk. Handles the full lifecycle: open file, drain buffer, close file, set timestamps/attributes.

- `startWriteFile(WriteBufferQueue* queue, const QString& path, const FileMetadata& meta)` - begin writing a new file
- `waitForCurrentFile()` - block until the current file is fully written (called at `SetOperationResult` time)
- Each decode worker owns exactly one `FileWriterThread` instance

```cpp
struct FileMetadata {
    FILETIME ctime, atime, mtime;
    DWORD attributes;
    bool hasTime = false;
    bool hasAttributes = false;
};
```

### 4. ExtractWorker (new: `extractworker.h/.cpp`)

A `QRunnable` submitted to a `QThreadPool`. Each worker:

- Opens its own `IInArchive` instance via `CommonHelper::tryOpenArchive()`
- Creates its own `FileWriterThread`
- Creates its own `ArchiveExtractCallBack` (modified for async IO)
- Calls `archive->Extract(indices, count, false, callback)` with its assigned file indices
- Reports per-worker progress via `QAtomicInteger` shared with the orchestrator

```cpp
class ExtractWorker : public QRunnable {
public:
    void setIndices(const QVector<UInt32>& indices);
    void setArchivePath(const QString& path);
    void setDestDirPath(const QString& destDir);
    void setPassword(const QString& password);
    void setProgressCounter(QAtomicInteger<quint64>* counter);
    void run() override;
};
```

### 5. Modify ArchiveExtractCallBack ([archiveextractcallback.h/.cpp](KeyZip/archiveextractcallback.h))

- `GetStream()`: pre-fetch file metadata (timestamps, attributes) from the archive. Create `WriteBufferQueue` + `AsyncOutStreamWrapper`. Signal `FileWriterThread` to start a new file.
- `SetOperationResult()`: call `m_bufferQueue->markFinished()` then `m_writerThread->waitForCurrentFile()` (small sync point per file, necessary for correctness). Remove the direct `SetFileTime`/`SetFileAttributes` calls -- those move to `FileWriterThread`.

### 6. Refactor ArchiveExtractor ([archiveextractor.h/.cpp](KeyZip/archiveextractor.h))

The orchestrator:

```cpp
void ArchiveExtractor::run() {
    // 1. Open archive, get item count
    // 2. Detect solid via kpidSolid
    // 3. Enumerate file indices (respecting m_entryPath filter)
    // 4. Partition indices based on solid flag:
    //    - Non-solid: split into N groups (N = QThread::idealThreadCount())
    //    - Solid: single group
    // 5. Create QThreadPool, submit ExtractWorkers
    // 6. Poll shared QAtomicInteger progress counter, emit updateProgress
    // 7. Wait for pool completion
    // 8. Emit extractSucceed / extractFailed
}
```

Progress polling: a timer loop (e.g., `QThread::msleep(200)`) reads the shared atomic counter and emits `updateProgress`. This replaces the current `BlockingQueuedConnection` progress signal which would block decode threads.

### 7. Update KeyZipWindow ([keyzipwindow.cpp](KeyZip/keyzipwindow.cpp))

- The `updateProgress` signal from `ArchiveExtractor` changes from `BlockingQueuedConnection` to `QueuedConnection` (since progress is now polled and emitted by the orchestrator, not by the decode callback directly).
- No other UI changes needed -- the `startArchiveExtractor()` interface stays the same.

### 8. Update CMakeLists.txt ([CMakeLists.txt](KeyZip/CMakeLists.txt))

Add new source files:

- `writebufferqueue.h`, `writebufferqueue.cpp`
- `asyncoutstreamwrapper.h`, `asyncoutstreamwrapper.cpp`
- `filewriterthread.h`, `filewriterthread.cpp`
- `extractworker.h`, `extractworker.cpp`

## Data Flow (After Refactoring)

```mermaid
sequenceDiagram
    participant SDK as 7z SDK
    participant Async as AsyncOutStreamWrapper
    participant Buf as WriteBufferQueue
    participant Writer as FileWriterThread
    participant Disk as QFile (Disk)

    SDK->>Async: GetStream(index)
    Async->>Buf: create buffer
    Async->>Writer: startWriteFile(buf, path, meta)
    
    par Decode and IO in parallel
        loop Decoded chunks
            SDK->>Async: Write(data)
            Async->>Buf: enqueue(data) [fast]
            Async-->>SDK: done [immediate return]
        end
    and
        loop Buffer drain
            Writer->>Buf: dequeue()
            Buf-->>Writer: data
            Writer->>Disk: QFile::write(data)
        end
    end

    SDK->>Async: SetOperationResult
    Async->>Buf: markFinished()
    Async->>Writer: waitForCurrentFile()
    Writer->>Disk: close + SetFileTime + SetFileAttributes
    Writer-->>Async: done
```



## Performance Considerations

- **Buffer size tuning**: Default 4MB per buffer. The 7z SDK typically produces 64KB-256KB chunks, so a 4MB buffer holds 16-64 chunks, giving the decoder ample runway before blocking.
- **Thread count**: Use `QThread::idealThreadCount()` for decode workers (capped at file count). For archives with very many small files, fewer threads may be more efficient to reduce overhead.
- **Memory bound**: With N decode workers and 4MB per buffer, peak additional memory is N * 4MB (e.g., 32MB for 8 workers). Acceptable.
- **Disk IO saturation**: Multiple writer threads will naturally saturate disk bandwidth. For HDD, too many parallel writers can cause seek thrashing -- consider limiting IO threads on spinning disks. For SSD, parallel writes are beneficial.

