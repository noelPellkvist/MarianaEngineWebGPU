#pragma once
#include <bitset>

enum class Key {
    UNKNOWN = -1,

    SPACE = 32,
    APOSTROPHE = 39,       // '
    COMMA = 44,            // ,
    MINUS = 45,            // -
    PERIOD = 46,           // .
    SLASH = 47,            // /
    KEY_0 = 48,
    KEY_1 = 49,
    KEY_2 = 50,
    KEY_3 = 51,
    KEY_4 = 52,
    KEY_5 = 53,
    KEY_6 = 54,
    KEY_7 = 55,
    KEY_8 = 56,
    KEY_9 = 57,
    SEMICOLON = 59,        // ;
    EQUAL = 61,            // =
    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    I = 73,
    J = 74,
    K = 75,
    L = 76,
    M = 77,
    N = 78,
    O = 79,
    P = 80,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    U = 85,
    V = 86,
    W = 87,
    X = 88,
    Y = 89,
    Z = 90,
    LEFT_BRACKET = 91,     // [
    BACKSLASH = 92,        
    RIGHT_BRACKET = 93,    // ]
    GRAVE_ACCENT = 96,     // `

    WORLD_1 = 161,         // non-US #1
    WORLD_2 = 162,         // non-US #2

    ESCAPE = 256,
    ENTER = 257,
    TAB = 258,
    BACKSPACE = 259,
    INSERT = 260,
    DELETE = 261,
    RIGHT = 262,
    LEFT = 263,
    DOWN = 264,
    UP = 265,
    PAGE_UP = 266,
    PAGE_DOWN = 267,
    HOME = 268,
    END = 269,

    CAPS_LOCK = 280,
    SCROLL_LOCK = 281,
    NUM_LOCK = 282,
    PRINT_SCREEN = 283,
    PAUSE = 284,

    F1 = 290,
    F2 = 291,
    F3 = 292,
    F4 = 293,
    F5 = 294,
    F6 = 295,
    F7 = 296,
    F8 = 297,
    F9 = 298,
    F10 = 299,
    F11 = 300,
    F12 = 301,
    F13 = 302,
    F14 = 303,
    F15 = 304,
    F16 = 305,
    F17 = 306,
    F18 = 307,
    F19 = 308,
    F20 = 309,
    F21 = 310,
    F22 = 311,
    F23 = 312,
    F24 = 313,
    F25 = 314,

    KP_0 = 320,
    KP_1 = 321,
    KP_2 = 322,
    KP_3 = 323,
    KP_4 = 324,
    KP_5 = 325,
    KP_6 = 326,
    KP_7 = 327,
    KP_8 = 328,
    KP_9 = 329,
    KP_DECIMAL = 330,
    KP_DIVIDE = 331,
    KP_MULTIPLY = 332,
    KP_SUBTRACT = 333,
    KP_ADD = 334,
    KP_ENTER = 335,
    KP_EQUAL = 336,

    LEFT_SHIFT = 340,
    LEFT_CONTROL = 341,
    LEFT_ALT = 342,
    LEFT_SUPER = 343,
    RIGHT_SHIFT = 344,
    RIGHT_CONTROL = 345,
    RIGHT_ALT = 346,
    RIGHT_SUPER = 347,
    MENU = 348
};

enum class MouseButton { Left=0, Right=1, Middle=2, Button4=3, Button5=4 };

struct GLFWwindow;

class IInput {
    public:
        IInput(GLFWwindow* win);

        //Update the input system, so it continues working
        void Update();

        /// Checks whether the specified key was pressed during the current frame.
        /// @param key The key to query.
        /// @return True if the key was pressed down this frame.
        bool IsKeyPressed(Key key) const;

        /// Checks whether the specified key was released during the current frame.
        /// @param key The key to query.
        /// @return True if the key was released this frame.
        bool IsKeyReleased(Key key) const;

        /// Checks whether the specified key is currently being held down.
        /// @param key The key to query.
        /// @return True if the key is currently held.
        bool IsKeyDown(Key key) const;


        /// Checks whether the specified mouse button was pressed during the current frame.
        /// @param button The mouse button to query.
        /// @return True if the mouse button was pressed down this frame.
        bool IsMouseButtonPressed(MouseButton button) const;
        
        /// Checks whether the specified mouse button was released during the current frame.
        /// @param button The mouse button to query.
        /// @return True if the mouse button was released this frame.
        bool IsMouseButtonReleased(MouseButton button) const;
        
        /// Checks whether the specified mouse button is currently being held down.
        /// @param button The mouse button to query.
        /// @return True if the mouse button is currently held.
        bool IsMouseButtonDown(MouseButton button) const;
        
        /// Retrieves the current mouse cursor position in screen coordinates.
        /// @param[out] x Reference to store the X position.
        /// @param[out] y Reference to store the Y position.
        void GetMousePosition(double& x, double& y) const;
        
        /// Retrieves the change in mouse position since the previous frame.
        /// @param[out] dx Reference to store the delta X value.
        /// @param[out] dy Reference to store the delta Y value.
        void GetMouseDelta(double& dx, double& dy) const;
        
        /// Retrieves the mouse scroll wheel delta since the previous frame.
        /// @param[out] dx Reference to store the horizontal scroll delta.
        /// @param[out] dy Reference to store the vertical scroll delta.
        void GetMouseScrollDelta(double& dx, double& dy) const;
        
        /// Locks or unlocks the mouse cursor within the application window.
        /// @param locked True to lock the cursor, false to unlock it.
        void SetCursorLocked(bool locked);
        
        /// Indicates whether the mouse cursor is currently locked within the application window.
        /// @return True if the cursor is locked, false otherwise.
        bool IsCursorLocked() const;


    private:
        static constexpr size_t KEY_CAP = 512;
        GLFWwindow* m_window;

        std::bitset<KEY_CAP> _keyPressed;
        std::bitset<KEY_CAP> _keyDown;
        std::bitset<KEY_CAP> _keyReleased;

        std::bitset<8> _mouseButtonPressed;
        std::bitset<8> _mouseButtonDown;
        std::bitset<8> _mouseButtonReleased;

        double _mx{0.0}, _my{0.0};
        double _mdx{0.0}, _mdy{0.0};
        double _sx{0.0}, _sy{0.0};

        bool _cursorLocked{false};

        void OnKey(Key key, int action);
        void OnMouseButton(int button, int action);
        void OnMouseMove(double x, double y);
        void OnMouseScroll(double xoffset, double yoffset);
};