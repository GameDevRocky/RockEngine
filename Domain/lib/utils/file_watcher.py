import os
import queue
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

# Event kinds put on the queue. The engine maps CHANGED to a hot-reload and
# DELETED to a "missing script" state, so the two must stay distinguishable --
# an earlier version enqueued bare paths and could only ever mean "changed".
CHANGED = "changed"
DELETED = "deleted"


class _EngineFileEventHandler(FileSystemEventHandler):
    def __init__(self, q: queue.Queue):
        super().__init__()
        self._queue = q

    def _enqueue(self, kind: str, path: str) -> None:
        normalized = os.path.normcase(os.path.abspath(path))
        self._queue.put((kind, normalized))

    def on_modified(self, event):
        # Raised by editors that write in-place.
        if not event.is_directory:
            self._enqueue(CHANGED, event.src_path)

    def on_created(self, event):
        # Raised by some editors on atomic save (write temp → create dest), and
        # by simply adding a new script file to the folder.
        if not event.is_directory:
            self._enqueue(CHANGED, event.src_path)

    def on_deleted(self, event):
        if not event.is_directory:
            self._enqueue(DELETED, event.src_path)

    def on_moved(self, event):
        # VS Code atomic save: writes a temp file then renames it to the real path.
        # watchdog reports this as a move event; dest_path is the actual file.
        #
        # A move is a delete of the source as much as a write of the destination,
        # which is also how a plain rename arrives -- so report both halves and
        # let the consumer decide. The engine re-checks the file on disk before
        # acting on a delete, so the temp-file half of an atomic save is harmless.
        if not event.is_directory:
            self._enqueue(DELETED, event.src_path)
            self._enqueue(CHANGED, event.dest_path)


class FileWatcher:
    _instance = None

    @classmethod
    def get(cls) -> "FileWatcher":
        if cls._instance is None:
            cls._instance = FileWatcher()
        return cls._instance

    def __init__(self):
        self._queue: queue.Queue = queue.Queue()
        self._handler = _EngineFileEventHandler(self._queue)
        self._observer = Observer()
        self._observer.daemon = True
        self._watched_dirs: set = set()

    def watch_directory(self, path: str) -> None:
        normalized = os.path.normcase(os.path.abspath(path))
        if normalized not in self._watched_dirs:
            if not self._observer.is_alive():
                self._observer.start()
            self._observer.schedule(self._handler, normalized, recursive=True)
            self._watched_dirs.add(normalized)

    def poll_changes(self) -> list:
        """Drain the queue, returning a list of ``(kind, path)`` tuples."""
        changes = []
        while True:
            try:
                changes.append(self._queue.get_nowait())
            except queue.Empty:
                break
        return changes
