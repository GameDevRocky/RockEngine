import math

class Vector2:
    def __init__(self, x=None, y=None):
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
    def length(self):
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

    def normalize(self):
        L = self.length()
        return Vector2(self.x / L, self.y / L) if L > 0 else Vector2(0, 0)

    # --- Boilerplate ---
    def __getitem__(self, index):
        return [self.x, self.y][index]

    def __iter__(self):
        yield self.x
        yield self.y

    def __repr__(self):
        return f"Vector2({self.x}, {self.y})"