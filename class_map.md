classDiagram
    class App {
    }
    class Document {
    }
    class Sprite {
    }
    class Widget {
    }
    class Window {
    }

    App --* MainWindow : owns
    App --> Workspace : uses
    App --> ContextBar : uses
    App --> Timeline : uses
    App --> Context : uses
    App --> Preferences : uses
    App --> Extensions : uses
    App --> DataRecovery : uses
    Document --* Sprite : contains
    Sprite --> Document : usedBy
    Window --|> Widget : inherits
    Widget --* Widget : contains