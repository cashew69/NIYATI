
struct GLFWwindow;

void InitGUI(GLFWwindow* window);
void NewFrameGUI();
void RenderGUI();
void ShutdownGUI();
void UpdateGUI(); // High-level update: NewFrame + Render Windows
