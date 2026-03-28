#pragma once

#include <functional>
#include <string>

namespace th {

using Task = std::function<void(int begin, int end, int workerId)>;

class ThreadStrategyContract {
public:
    virtual ~ThreadStrategyContract() = default;

    // Nombre del modelo de ejecucion.
    virtual std::string name() const = 0;

    // Configuracion inicial de workers/scheduler.
    virtual void configure(int workers) = 0;

    // Ejecuta una tarea de rango [0, itemCount) segun la estrategia.
    virtual void runForRange(int itemCount, const Task& task) = 0;

    // Punto de sincronizacion entre etapas cuando aplique.
    virtual void barrier() = 0;
};

} // namespace th
