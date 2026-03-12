#include "visualization/Renderer.h"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <string>
#include <memory>
#include <cmath>
#include <sstream>
#include <iomanip>

// ============================================================
// Renderer — SFML 2D visualisation of SPH particles
//
// Particle colour is mapped from speed:
//   slow → blue,  medium → cyan,  fast → white/yellow
// ============================================================

static const char* FONT_PATHS[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansMono-Bold.ttf",
    "/usr/share/fonts/truetype/ubuntu/UbuntuSansMono[wght].ttf",
    nullptr
};

Renderer::Renderer() {
    window_ = std::make_unique<sf::RenderWindow>(
        sf::VideoMode(Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT),
        "SPH Fluid Simulation",
        sf::Style::Titlebar | sf::Style::Close);
    window_->setFramerateLimit(60);

    // Try to load a monospace font for the HUD
    for (int i = 0; FONT_PATHS[i] != nullptr; ++i) {
        if (font_.loadFromFile(FONT_PATHS[i])) {
            fontLoaded_ = true;
            break;
        }
    }
}

bool Renderer::isOpen() const {
    return window_ && window_->isOpen();
}

void Renderer::handleEvents() {
    sf::Event event;
    while (window_->pollEvent(event)) {
        if (event.type == sf::Event::Closed)
            window_->close();
    }
}

// ------------------------------------------------------------------
// Internal: draw particles into a sub-region
// ------------------------------------------------------------------
void Renderer::drawParticles(const std::vector<Particle>& particles,
                             float domainW, float domainH,
                             float offsetX, float offsetY,
                             float viewW, float viewH) {
    float scaleX = viewW / domainW;
    float scaleY = viewH / domainH;

    float maxSpeed = 1.0f;
    for (const auto& p : particles)
        maxSpeed = std::max(maxSpeed, p.velocity.length());

    float radius = Config::PARTICLE_RENDER_RADIUS;
    sf::CircleShape circle(radius);
    circle.setOrigin(radius, radius);

    for (const auto& p : particles) {
        float speed = p.velocity.length();
        float t = std::min(speed / maxSpeed, 1.0f);

        // Colour gradient: blue → cyan → white
        sf::Uint8 r = static_cast<sf::Uint8>(30 + 225 * t * t);
        sf::Uint8 g = static_cast<sf::Uint8>(100 + 155 * t);
        sf::Uint8 b = static_cast<sf::Uint8>(200 + 55 * (1.0f - t));
        circle.setFillColor(sf::Color(r, g, b));

        float screenX = offsetX + p.position.x * scaleX;
        float screenY = offsetY + viewH - p.position.y * scaleY;
        circle.setPosition(screenX, screenY);
        window_->draw(circle);
    }

    // Domain border
    sf::RectangleShape border(sf::Vector2f(viewW - 2, viewH - 2));
    border.setPosition(offsetX + 1, offsetY + 1);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(sf::Color(80, 80, 80));
    border.setOutlineThickness(1.0f);
    window_->draw(border);
}

// ------------------------------------------------------------------
// Full-window draw with HUD overlay
// ------------------------------------------------------------------
void Renderer::draw(const std::vector<Particle>& particles,
                    float domainW, float domainH,
                    int stepNum, const std::string& modelName,
                    double msPerStep, int numThreads) {
    window_->clear(sf::Color(20, 20, 30));

    float hudHeight = fontLoaded_ ? 50.0f : 0.0f;
    float viewW = static_cast<float>(Config::WINDOW_WIDTH);
    float viewH = static_cast<float>(Config::WINDOW_HEIGHT) - hudHeight;

    drawParticles(particles, domainW, domainH, 0.0f, hudHeight, viewW, viewH);

    // HUD overlay
    if (fontLoaded_) {
        // Background bar
        sf::RectangleShape hudBg(sf::Vector2f(viewW, hudHeight));
        hudBg.setFillColor(sf::Color(10, 10, 15, 230));
        window_->draw(hudBg);

        // Separator line
        sf::RectangleShape sep(sf::Vector2f(viewW, 2.0f));
        sep.setPosition(0, hudHeight - 2);
        sep.setFillColor(sf::Color(60, 120, 200));
        window_->draw(sep);

        // Model name and thread count
        sf::Text modelText;
        modelText.setFont(font_);
        modelText.setCharacterSize(18);
        modelText.setFillColor(sf::Color(100, 200, 255));
        modelText.setPosition(12, 6);
        modelText.setString(modelName + "  |  " + std::to_string(numThreads) + " thread" +
                            (numThreads > 1 ? "s" : ""));
        window_->draw(modelText);

        // Step + timing info
        std::ostringstream oss;
        oss << "Step " << stepNum;
        if (msPerStep > 0.0)
            oss << "  |  " << std::fixed << std::setprecision(2) << msPerStep << " ms/step";
        oss << "  |  " << particles.size() << " particles";

        sf::Text infoText;
        infoText.setFont(font_);
        infoText.setCharacterSize(16);
        infoText.setFillColor(sf::Color(180, 180, 190));
        infoText.setPosition(12, 28);
        infoText.setString(oss.str());
        window_->draw(infoText);

        // Controls hint (right-aligned)
        sf::Text hintText;
        hintText.setFont(font_);
        hintText.setCharacterSize(14);
        hintText.setFillColor(sf::Color(100, 100, 110));
        hintText.setString("Close window to stop");
        float hintW = hintText.getLocalBounds().width;
        hintText.setPosition(viewW - hintW - 12, 30);
        window_->draw(hintText);
    }

    window_->display();
}

// ------------------------------------------------------------------
// Split-screen region draw (for side-by-side comparison)
// ------------------------------------------------------------------
void Renderer::drawRegion(const std::vector<Particle>& particles,
                          float domainW, float domainH,
                          const sf::FloatRect& viewport,
                          const std::string& label,
                          double msPerStep) {
    drawParticles(particles, domainW, domainH,
                  viewport.left, viewport.top,
                  viewport.width, viewport.height);

    // Label at top of region
    if (fontLoaded_) {
        // Semi-transparent label background
        sf::RectangleShape labelBg(sf::Vector2f(viewport.width, 28.0f));
        labelBg.setPosition(viewport.left, viewport.top);
        labelBg.setFillColor(sf::Color(10, 10, 15, 200));
        window_->draw(labelBg);

        sf::Text text;
        text.setFont(font_);
        text.setCharacterSize(15);
        text.setFillColor(sf::Color(100, 200, 255));
        text.setPosition(viewport.left + 8, viewport.top + 4);

        std::ostringstream oss;
        oss << label;
        if (msPerStep > 0.0)
            oss << "  —  " << std::fixed << std::setprecision(2) << msPerStep << " ms/step";
        text.setString(oss.str());
        window_->draw(text);
    }
}
