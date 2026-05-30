from rock_engine.rendering import material_module
from ...utils.re_math import Vector2, Vector3, Vector4


class Material:
    def __init__(self, material_id: str):
        self.id = material_id

    @property
    def name(self) -> str:
        return material_module.get_name(self.id)

    @name.setter
    def name(self, value: str):
        material_module.set_name(self.id, value)

    @property
    def shader_id(self) -> str:
        return material_module.get_shader_id(self.id)

    @shader_id.setter
    def shader_id(self, value: str):
        material_module.set_shader(self.id, value)

    def set_float(self, name: str, value: float):
        material_module.set_float(self.id, name, float(value))

    def set_vec2(self, name: str, value):
        v = tuple(value)
        material_module.set_vec2(self.id, name, float(v[0]), float(v[1]))

    def set_vec3(self, name: str, value):
        v = tuple(value)
        material_module.set_vec3(self.id, name, float(v[0]), float(v[1]), float(v[2]))

    def set_vec4(self, name: str, value):
        v = tuple(value)
        material_module.set_vec4(self.id, name, float(v[0]), float(v[1]), float(v[2]), float(v[3]))

    def set_texture(self, name: str, tex_id: str):
        material_module.set_texture(self.id, name, tex_id)
