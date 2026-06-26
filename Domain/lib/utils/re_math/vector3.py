import math

class Vector3:
    def __init__(self, x=None, y=None, z=None):
        if x is None:
            self.x = 0.0
            self.y = 0.0
            self.z = 0.0
        elif isinstance(x, (Vector3, tuple, list)):
            self.x = float(x[0])
            self.y = float(x[1])
            self.z = float(x[2])
        elif y is None:
            self.x = float(x)
            self.y = float(x)
            self.z = float(x)
        elif z is None:
            self.x = float(x)
            self.y = float(y)
            self.z = 0.0
        else:
            self.x = float(x)
            self.y = float(y)
            self.z = float(z)

    def __add__(self, other):
        if isinstance(other, Vector3):
            return Vector3(self.x + other.x, self.y + other.y, self.z + other.z)
        return Vector3(self.x + other[0], self.y + other[1], self.z + other[2])

    def __sub__(self, other):
        if isinstance(other, Vector3):
            return Vector3(self.x - other.x, self.y - other.y, self.z - other.z)
        return Vector3(self.x - other[0], self.y - other[1], self.z - other[2])

    def __iadd__(self, other):
        if isinstance(other, Vector3):
            self.x += other.x
            self.y += other.y
            self.z += other.z
        else:
            self.x += other[0]
            self.y += other[1]
            self.z += other[2]
        return self

    def __mul__(self, scalar):
        return Vector3(self.x * scalar, self.y * scalar, self.z * scalar)

    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)

    def dot(self, other):
        return self.x * other.x + self.y * other.y + self.z * other.z

    def cross(self, other):
        return Vector3(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x
        )

    def distance_to(self, other):
        return math.sqrt((self.x - other.x)**2 + (self.y - other.y)**2 + (self.z - other.z)**2)

    def normalize(self):
        L = self.length()
        return Vector3(self.x / L, self.y / L, self.z / L) if L > 0 else Vector3(0, 0, 0)

    def __getitem__(self, index):
        return [self.x, self.y, self.z][index]

    def __iter__(self):
        yield self.x
        yield self.y
        yield self.z

    def __repr__(self):
        return f"Vector3({self.x}, {self.y}, {self.z})"
