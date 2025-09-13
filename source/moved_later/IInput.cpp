#include <moved_later/IInput.hpp>
#include <GLFW/glfw3.h>
#include <cstddef>   // size_t (optional on MSVC but nice to have)

// --- ctor: wire up callbacks & initial state ---
IInput::IInput(GLFWwindow* win)
    : m_window(win)
{
    glfwSetWindowUserPointer(m_window, this);

    // We track edges ourselves
    glfwSetInputMode(m_window, GLFW_STICKY_KEYS, GLFW_FALSE);
    glfwSetInputMode(m_window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_FALSE);

    // Raw mouse motion helps when cursor is disabled
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // Initial mouse position
    glfwGetCursorPos(m_window, &_mx, &_my);

    // --- Callbacks ---
    glfwSetKeyCallback(m_window, [](GLFWwindow* w, int key, int sc, int action, int mods){
        (void)sc; (void)mods;
        auto* self = static_cast<IInput*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        self->OnKey(static_cast<Key>(key), action);
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* w, int button, int action, int mods){
        (void)mods;
        auto* self = static_cast<IInput*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        self->OnMouseButton(button, action);
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* w, double x, double y){
        auto* self = static_cast<IInput*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        self->OnMouseMove(x, y);
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* w, double xoff, double yoff){
        auto* self = static_cast<IInput*>(glfwGetWindowUserPointer(w));
        if (!self) return;
        self->OnMouseScroll(xoff, yoff);
    });
}

// --- per-frame housekeeping (call once after glfwPollEvents) ---
void IInput::Update()
{
    _keyPressed.reset();
    _keyReleased.reset();
    _mouseButtonPressed.reset();
    _mouseButtonReleased.reset();

    _mdx = _mdy = 0.0;
    _sx  = _sy  = 0.0;
}

// ---------- Queries: keys ----------
bool IInput::IsKeyDown(Key key) const
{
    int vk = static_cast<int>(key);
    if (vk < 0 || vk >= static_cast<int>(KEY_CAP)) return false;
    return _keyDown.test(static_cast<size_t>(vk));
}

bool IInput::IsKeyPressed(Key key) const
{
    int vk = static_cast<int>(key);
    if (vk < 0 || vk >= static_cast<int>(KEY_CAP)) return false;
    return _keyPressed.test(static_cast<size_t>(vk));
}

bool IInput::IsKeyReleased(Key key) const
{
    int vk = static_cast<int>(key);
    if (vk < 0 || vk >= static_cast<int>(KEY_CAP)) return false;
    return _keyReleased.test(static_cast<size_t>(vk));
}

// ---------- Queries: mouse buttons ----------
bool IInput::IsMouseButtonDown(MouseButton b) const
{
    int i = static_cast<int>(b);
    if (i < 0 || i >= 8) return false;
    return _mouseButtonDown.test(static_cast<size_t>(i));
}

bool IInput::IsMouseButtonPressed(MouseButton b) const
{
    int i = static_cast<int>(b);
    if (i < 0 || i >= 8) return false;
    return _mouseButtonPressed.test(static_cast<size_t>(i));
}

bool IInput::IsMouseButtonReleased(MouseButton b) const
{
    int i = static_cast<int>(b);
    if (i < 0 || i >= 8) return false;
    return _mouseButtonReleased.test(static_cast<size_t>(i));
}

// ---------- Queries: mouse movement / scroll ----------
void IInput::GetMousePosition(double& x, double& y) const
{
    x = _mx; y = _my;
}

void IInput::GetMouseDelta(double& dx, double& dy) const
{
    dx = _mdx; dy = _mdy;
}

void IInput::GetMouseScrollDelta(double& dx, double& dy) const
{
    dx = _sx; dy = _sy;
}

// ---------- Cursor lock ----------
void IInput::SetCursorLocked(bool locked)
{
    _cursorLocked = locked;
    glfwSetInputMode(m_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    // Reset deltas to avoid a big jump on mode switch
    double x, y;
    glfwGetCursorPos(m_window, &x, &y);
    _mx = x; _my = y;
    _mdx = _mdy = 0.0;
}

bool IInput::IsCursorLocked() const
{
    return _cursorLocked;
}

// ---------- Event handlers (internal) ----------
void IInput::OnKey(Key key, int action)
{
    int vk = static_cast<int>(key);
    if (vk < 0 || vk >= static_cast<int>(KEY_CAP)) return;
    size_t i = static_cast<size_t>(vk);

    if (action == GLFW_PRESS) {
        if (!_keyDown.test(i)) _keyPressed.set(i);
        _keyDown.set(i);
    } else if (action == GLFW_RELEASE) {
        if (_keyDown.test(i)) _keyReleased.set(i);
        _keyDown.reset(i);
    } else {
        // GLFW_REPEAT -> ignore for edges; _keyDown already true
    }
}

void IInput::OnMouseButton(int button, int action)
{
    if (button < 0 || button >= 8) return;
    size_t i = static_cast<size_t>(button);

    if (action == GLFW_PRESS) {
        if (!_mouseButtonDown.test(i)) _mouseButtonPressed.set(i);
        _mouseButtonDown.set(i);
    } else if (action == GLFW_RELEASE) {
        if (_mouseButtonDown.test(i)) _mouseButtonReleased.set(i);
        _mouseButtonDown.reset(i);
    }
}

void IInput::OnMouseMove(double x, double y)
{
    // accumulate delta for this frame
    _mdx += (x - _mx);
    _mdy += (y - _my);
    _mx = x; _my = y;
}

void IInput::OnMouseScroll(double xoffset, double yoffset)
{
    _sx += xoffset;
    _sy += yoffset;
}
