import os
import queue
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler


class _EngineFileEventHandler(FileSystemEventHandler):
    def __init__(self, q: queue.Queue):
        super().__init__()
        self._queue = q

    def _enqueue(self, path: str) -> None:
        normalized = os.path.normcase(os.path.abspath(path))
        self._queue.put(normalized)

    def on_modified(self, event):
        # Raised by editors that write in-place.
        if not event.is_directory:
            self._enqueue(event.src_path)

    def on_created(self, event):
        # Raised by some editors on atomic save (write temp → create dest).
        if not event.is_directory:
            self._enqueue(event.src_path)

    def on_moved(self, event):
        # VS Code atomic save: writes a temp file then renames it to the real path.
        # watchdog reports this as a move event; dest_path is the actual file.
        if not event.is_directory:
            self._enqueue(event.dest_path)


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
        changes = []
        while True:
            try:
                changes.append(self._queue.get_nowait())
            except queue.Empty:
                break
        return changes
