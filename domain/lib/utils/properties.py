class Range:
    """Metadata descriptor for numeric property constraints."""
    def __init__(self, min=None, max=None):
        self.min = min
        self.max = max


class Step:
    """Metadata descriptor for property step size."""
    def __init__(self, value=0.1):
        self.value = value


class Tooltip:
    """Metadata descriptor for property tooltip text."""
    def __init__(self, text):
        self.text = text
