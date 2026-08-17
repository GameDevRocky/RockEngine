from rock_engine.components import component_action_module

# Field separator inside one encoded call entry. Chosen because it cannot occur
# in a UUID or a Python identifier, which is what the first three fields are.
SEP = "|"

# How many separators to split on. The argument is LAST and is arbitrary user
# text, so it is the only field allowed to contain the separator itself —
# splitting with a limit keeps "say|this" intact instead of truncating it.
_FIELDS = 4


def encode(object_id="", component_id="", method="", raw_arg=""):
    """Pack one call into its serialized form.

    ``<objectId>|<componentId>|<method>|<argument>``

    Both ids are stored: the component id is what actually dispatches (a
    GameObject can hold several scripts, so its own id cannot say which one you
    meant), while the object id is what the editor's target picker binds to and
    what it needs to report a target that has since been deleted.
    """
    return SEP.join([object_id or "", component_id or "", method or "",
                     "" if raw_arg is None else str(raw_arg)])


def decode(entry):
    """Unpack one entry into (object_id, component_id, method, raw_arg)."""
    parts = str(entry).split(SEP, _FIELDS - 1)
    parts += [""] * (_FIELDS - len(parts))
    return parts[0], parts[1], parts[2], parts[3]


class Event(list):
    """A list of editor-wired calls that a script can fire.

    Declared on a script as a field and populated in the Inspector::

        class Enemy(ScriptableComponent):
            on_death: Event

            def die(self):
                self.on_death.invoke()

    Subclassing ``list`` is deliberate rather than incidental: an event IS its
    ordered list of entries, and being a list means the engine's existing
    ``list[str]`` marshalling serializes it, deep-copies it into play mode, and
    edits it in the Inspector with no separate code path. The entries are opaque
    encoded strings — use ``add``/``decode`` rather than reading them directly.
    """

    def invoke(self):
        """Fire every wired call, in the order the Inspector shows them.

        One failing entry does not stop the rest: a broken wire is a content
        bug, and swallowing the whole event because the third of five targets
        was deleted would hide the four that still work. Failures are reported
        to the console by the engine side.
        """
        for entry in list(self):
            _, component_id, method, raw_arg = decode(entry)
            if not component_id or not method:
                continue   # a half-configured row in the Inspector, not an error
            component_action_module.invoke(component_id, method, raw_arg)

    def add(self, component_id, method, raw_arg="", object_id=""):
        """Wire a call from script code, the same shape the Inspector writes."""
        self.append(encode(object_id, component_id, method, raw_arg))

    def clear_targets(self):
        """Drop every wired call."""
        del self[:]
