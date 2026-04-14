import math

class Vector4:
    def __init__(self, x=None, y=None, z=None, w=None):
        if x is None:
            self.x = 0.0
            self.y = 0.0
            self.z = 0.0
            self.w = 0.0
        elif isinstance(x, (Vector4, tuple, list)):
            self.x = float(x[0])
            self.y = float(x[1])
            self.z = float(x[2])
            self.w = float(x[3])
        elif y is None:
            self.x = float(x)
            self.y = float(x)
            self.z = float(x)
            self.w = float(x)
        elif z is None:
            self.x = float(x)
            self.y = float(y)
            self.z = 0.0
            self.w = 0.0
        elif w is None:
            self.x = float(x)
            self.y = float(y)
            self.z = float(z)
            self.w = 0.0
        else:
            self.x = float(x)
            self.y = float(y)
            self.z = float(z)
            self.w = float(w)

    def __add__(self, other):
        if isinstance(other, Vector4):
            return Vector4(self.x + other.x, self.y + other.y, self.z + other.z, self.w + other.w)
        return Vector4(self.x + other[0], self.y + other[1], self.z + other[2], self.w + other[3])

    def __sub__(self, other):
        if isinstance(other, Vector4):
            return Vector4(self.x - other.x, self.y - other.y, self.z - other.z, self.w - other.w)
        return Vector4(self.x - other[0], self.y - other[1], self.z - other[2], self.w - other[3])

    def __iadd__(self, other):
        if isinstance(other, Vector4):
            self.x += other.x
            self.y += other.y
            self.z += other.z
            self.w += other.w
        else:
            self.x += other[0]
            self.y += other[1]
            self.z += other[2]
            self.w += other[3]
        return self

    def __mul__(self, scalar):
        return Vector4(self.x * scalar, self.y * scalar, self.z * scalar, self.w * scalar)

    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2 + self.w**2)

    def dot(self, other):
        return self.x * other.x + self.y * other.y + self.z * other.z + self.w * other.w

    def distance_to(self, other):
        return math.sqrt((self.x - other.x)**2 + (self.y - other.y)**2 + (self.z - other.z)**2 + (self.w - other.w)**2)

    def normalize(self):
        L = self.length()
        return Vector4(self.x / L, self.y / L, self.z / L, self.w / L) if L > 0 else Vector4(0, 0, 0, 0)

    def __getitem__(self, index):
        return [self.x, self.y, self.z, self.w][index]

    def __iter__(self):
        yield self.x
        yield self.y
        yield self.z
        yield self.w

    def __repr__(self):
        return f"Vector4({self.x}, {self.y}, {self.z}, {self.w})"
