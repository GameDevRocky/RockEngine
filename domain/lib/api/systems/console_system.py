from rock_engine.systems import console_module




class Console:
    @staticmethod
    def comment(message):
       console_module.comment(str(message))
    
    @staticmethod
    def warn(message):
        console_module.warn(str(message))
    
    @staticmethod
    def alert(message):
        console_module.alert(str(message))