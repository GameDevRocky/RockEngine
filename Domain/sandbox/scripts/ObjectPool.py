"""Object pool for reusing GameObjects instead of creating/destroying them.

Usage:
    1. Create a GameObject with this script attached
    2. Assign a prefab (disabled GameObject template) to the prefab field
    3. Set the initial pool size
    4. Call get() to retrieve an object from the pool
    5. Call return_to_pool() to return an object when done

The pool creates all objects in awake() and reuses them, avoiding runtime allocation.
Objects returned to the pool are deactivated and stored for the next get() call.
"""

from Domain import *


class ObjectPool(ScriptableComponent):

    # The template object to clone. Must be disabled in the scene.
    prefab : GameObject = None

    # How many objects to pre-create at startup
    initial_size : Reflect[int, Slider, Step(1), Range(1, 500),
                           Tooltip("Number of objects to pre-create in awake()")] = 50

    # Allow the pool to grow if all objects are in use
    allow_growth : Reflect[bool, Tooltip("Create new objects if pool is empty")] = True

    def awake(self):
        """Pre-create the pool of objects."""
        self._available = set()    # Inactive GameObject handles ready to use
        self._in_use = set()       # Active GameObject handles currently deployed

        if not self.prefab:
            Console.warn("ObjectPool: no prefab assigned. Pool will be empty.")
            return

        # Ensure the prefab itself is disabled
        self.prefab.active = False

        # Pre-create all pool objects
        for i in range(self.initial_size):
            obj = self._create_new_object()
            if obj:
                self._available.add(obj)

    def _create_new_object(self):
        """Create a new object from the prefab."""
        obj = self.duplicate(self.prefab)
        if not obj:
            Console.warn("ObjectPool: failed to duplicate prefab.")
            return None

        # Cut it loose from any parent hierarchy
        obj.transform.parent = None

        # Keep it inactive until requested
        obj.active = False

        return obj

    def get(self):
        """Get an object from the pool. Returns None if pool is empty and growth is disabled."""
        # Try to reuse an available object
        if self._available:
            obj = self._available.pop()
            self._in_use.add(obj)
            return obj

        # Pool is empty - grow if allowed
        if self.allow_growth:
            Console.comment(f"ObjectPool: growing pool (current size: {len(self._in_use)})")
            obj = self._create_new_object()
            if obj:
                self._in_use.add(obj)
            return obj

        # Pool exhausted and growth disabled
        Console.warn("ObjectPool: pool exhausted and growth disabled.")
        return None

    def return_to_pool(self, obj):
        """Return an object to the pool for reuse."""
        if not obj:
            return

        # Move from in_use to available
        self._in_use.discard(obj)
        self._available.add(obj)

        # Deactivate the object to hide it
        obj.active = False

        # Reset its position to prevent it from being visible at the death location
        obj.transform.world_position = Vector2(-10000, -10000)

    def get_pool_stats(self):
        """Return (available_count, in_use_count) for debugging."""
        return (len(self._available), len(self._in_use))
