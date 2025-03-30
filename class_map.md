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
    class Context {
    }
    class Command {
    }
    class Cel {
    }
    class Layer {
    }
    class Image {
    }
    class Palette {
    }
    class Mask {
    }
    class Tileset {
    }
    class Slice {
    }
    class Tag {
    }
    class Button {
    }
    class Label {
    }
    class Entry {
    }
    class Slider {
    }
    class Panel {
    }
    class Menu {
    }
    class ComboBox {
    }
    class ListBox {
    }
    class Display {
    }
    class Manager {
    }
    class Graphics {
    }
    class Event {
    }
    class UISystem {
    }
    class AppMod {
    }
    class InputChain {
    }
    class RecentFiles {
    }
    class AppBrushes {
    }
    class BackupIndicator {
    }
    class "Script::Engine" {
    }
    class Object {
    }
    class WithUserData {
    }
    class Component {
    }
    class Theme {
    }
    class Style {
    }
    class ButtonBase {
    }

    App --* UISystem : owns
    App --> AppMod : uses
    App --> InputChain : uses
    App --> RecentFiles : uses
    App --* AppBrushes : owns
    App --* BackupIndicator : owns
    App --* "Script::Engine" : owns (Conditional)
    Document --|> Object : inherits
    Sprite --|> WithUserData : inherits
    Widget --|> Component : inherits
    Widget --> Theme : uses
    Widget --> Style : uses
    Widget --> Manager : uses
    Widget --> Display : uses
    Window --> Display : uses
    Window --> ButtonBase : uses
    Window --> Label : uses
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