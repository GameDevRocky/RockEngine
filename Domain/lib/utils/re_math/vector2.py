import math

class Vector2:
    # A vector from a native property can carry its get/set pair. Standalone
    # vectors keep their coordinates locally. Neither form needs a __dict__.
    __slots__ = ('_x', '_y', '_binding')

    def __init__(self, x=None, y=None):
        self._binding = None
        # Case: Vector2() -> (0.0, 0.0)
        if x is None:
            self.x = 0.0
            self.y = 0.0
            
        # Case: Vector2(other_vec) or Vector2((x, y))
        elif isinstance(x, (Vector2, tuple, list)):
            self.x = float(x[0])
            self.y = float(x[1])
            
        # Case: Vector2(val) -> (val, val) [Uniform Scale/Pos]
        elif y is None:
            self.x = float(x)
            self.y = float(x)
            
        # Case: Vector2(x, y) -> (x, y)
        else:
            self.x = float(x)
            self.y = float(y)

    @classmethod
    def _bound(cls, getter, setter, object_id):
        vector = cls()
        vector._binding = (getter, setter, object_id)
        return vector

    @property
    def x(self):
        if self._binding is None:
            return self._x
        getter, _, object_id = self._binding
        return getter(object_id)[0]

    @x.setter
    def x(self, value):
        if self._binding is None:
            self._x = value
            return
        getter, setter, object_id = self._binding
        _, y = getter(object_id)
        setter(object_id, float(value), y)

    @property
    def y(self):
        if self._binding is None:
            return self._y
        getter, _, object_id = self._binding
        return getter(object_id)[1]

    @y.setter
    def y(self, value):
        if self._binding is None:
            self._y = value
            return
        getter, setter, object_id = self._binding
        x, _ = getter(object_id)
        setter(object_id, x, float(value))

    # --- Math Operators ---
    def __add__(self, other):
        if isinstance(other, Vector2):
            return Vector2(self.x + other.x, self.y + other.y)
        return Vector2(self.x + other[0], self.y + other[1])

    def __sub__(self, other):
        if isinstance(other, Vector2):
            return Vector2(self.x - other.x, self.y - other.y)
        return Vector2(self.x - other[0], self.y - other[1])


    def __iadd__(self, other):
        if isinstance(other, Vector2):
            self.x += other.x
            self.y += other.y
        else:
            self.x += other[0]
            self.y += other[1]
        return self

    def __mul__(self, scalar):
        return Vector2(self.x * scalar, self.y * scalar)

    def __neg__(self):
        return Vector2(-self.x, -self.y)

    # --- Geometry Logic ---
    @property
    def magnitude(self):
        return math.sqrt(self.x**2 + self.y**2)

    def dot(self, other):
        return self.x * other.x + self.y * other.y

    def cross(self, other):
        # 2D cross product magnitude
        return self.x * other.y - self.y * other.x

    def distance_to(self, other):
        return math.sqrt((self.x - other.x)**2 + (self.y - other.y)**2)

    def angle_to(self, other):
        # Returns signed angle in degrees
        angle_rad = math.atan2(self.cross(other), self.dot(other))
        return math.degrees(angle_rad)

    def normalized(self):
        L = self.magnitude
        return Vector2(self.x / L, self.y / L) if L > 0 else Vector2(0, 0)


    def normalize(self):
        L = self.magnitude
        return Vector2(self.x / L, self.y / L) if L > 0 else Vector2(0, 0)

    # --- Boilerplate ---
    def __getitem__(self, index):
        return [self.x, self.y][index]

    def __iter__(self):
        yield self.x
        yield self.y

    def __repr__(self):
        return f"Vector2({self.x}, {self.y})"
