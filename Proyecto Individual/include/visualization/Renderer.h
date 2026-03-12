#pragma once

#include <SFML/Graphics.hpp>

#include "core/Particle.h"
#include "Config.h"
#include <vector>
#include <string>
#include <memory>

// ============================================================
// Renderer — draws the 2D particle simulation using SFML
// ============================================================
class Renderer {
public:
    Renderer();

    /// Returns false when the window has been closed.
    bool isOpen() const;

    /// Process window events (close, etc.).
    void handleEvents();

    /// Draw all particles with full-window HUD.
    void draw(const std::vector<Particle>& particles,
              float domainW, float domainH,
              int stepNum, const std::string& modelName,
              double msPerStep = 0.0, int numThreads = 1);

    /// Draw particles into a sub-region of the window (for split-screen).
    void drawRegion(const std::vector<Particle>& particles,
                    float domainW, float domainH,
                    const sf::FloatRect& viewport,
                    const std::string& label,
                    double msPerStep = 0.0);

    /// Access the window for manual clear/display control.
    sf::RenderWindow& window() { return *window_; }

    /// Get the loaded font (for external text drawing).
    const sf::Font& font() const { return font_; }

private:
    std::unique_ptr<sf::RenderWindow> window_;
    sf::Font font_;
    bool fontLoaded_ = false;

    void drawParticles(const std::vector<Particle>& particles,
                       float domainW, float domainH,
                       float offsetX, float offsetY,
                       float viewW, float viewH);
};
