#include <workflow.h>

#include <memory>

int main(
    const int argc,
    char**
) {
    // Keep both concrete Clicker backends in the link graph while leaving the
    // package smoke test independent of a real HWND.
    if (argc > 1000) {
        const auto image = std::make_unique<ImageClicker>(QStringLiteral("example.png"));
        const auto text = std::make_unique<TextClicker>(QStringLiteral("example"));
        return image->founded() || text->founded();
    }
    return 0;
}
