#include "Reflection.h"

#include <chrono>
#include <iostream>
#include <string>

class BenchObject
{
public:
    int add(int a, int b) { return a + b; }

    int value_ = 0;
};

typedef std::chrono::high_resolution_clock Clock;

double elapsedMs(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
               end - start)
        .count();
}

void benchmarkBulkRegistration(std::size_t count)
{
    Evently::ReflectionRegistry &registry =
        Evently::ReflectionRegistry::getInstance();

    const Clock::time_point start = Clock::now();
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string name = "Bench" + std::to_string(index);
        registry.registerClass<BenchObject>(name);
        registry.registerField<BenchObject>(name, "value", &BenchObject::value_);
        registry.registerMethod<BenchObject, int, int, int>(
            name, "add", &BenchObject::add);
    }
    const Clock::time_point end = Clock::now();
    std::cout << "bulk-register " << count << " classes: "
              << elapsedMs(start, end) << " ms\n";
}

void benchmarkBatchRegistration(std::size_t count)
{
    Evently::ReflectionRegistry &registry =
        Evently::ReflectionRegistry::getInstance();

    const Clock::time_point start = Clock::now();
    {
        Evently::RegistrationBatch batch(registry);
        for (std::size_t index = 0; index < count; ++index)
        {
            const std::string name = "BatchBench" + std::to_string(index);
            registry.registerClass<BenchObject>(name);
            registry.registerField<BenchObject>(name, "value",
                                                &BenchObject::value_);
            registry.registerMethod<BenchObject, int, int, int>(
                name, "add", &BenchObject::add);
        }
        batch.commit();
    }
    const Clock::time_point end = Clock::now();
    std::cout << "batch-register " << count << " classes: "
              << elapsedMs(start, end) << " ms\n";
}

void benchmarkQueries(std::size_t count)
{
    Evently::ReflectionRegistry &registry =
        Evently::ReflectionRegistry::getInstance();

    const Clock::time_point start = Clock::now();
    std::size_t totalFields = 0;
    std::size_t totalMethods = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string name = "Bench" + std::to_string(index);
        totalFields += registry.getFields(name).size();
        totalMethods += registry.getMethods(name).size();
    }
    const Clock::time_point end = Clock::now();
    std::cout << "query " << count << " classes: " << elapsedMs(start, end)
              << " ms (" << totalFields << " fields, " << totalMethods
              << " methods)\n";
}

void benchmarkInvoke(std::size_t count)
{
    Evently::ReflectionRegistry &registry =
        Evently::ReflectionRegistry::getInstance();

    Evently::ObjectHandle handle = registry.createInstance("Bench0");
    BenchObject *object = handle.get<BenchObject>();
    if (object == nullptr)
    {
        return;
    }

    const Clock::time_point start = Clock::now();
    int sum = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        sum += Evently::any_cast<int>(registry.invokeMethod(
            "Bench0", "add", object,
            {Evently::Any(static_cast<int>(index)), Evently::Any(1)}));
    }
    const Clock::time_point end = Clock::now();
    std::cout << "invoke " << count << " methods: " << elapsedMs(start, end)
              << " ms (sum=" << sum << ")\n";
}

void benchmarkAny(std::size_t count)
{
    const Clock::time_point start = Clock::now();
    int sum = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        Evently::Any local(static_cast<int>(index));
        Evently::Any copied(local);
        Evently::Any moved(std::move(copied));
        sum += Evently::any_cast<int>(moved);
    }
    const Clock::time_point end = Clock::now();
    std::cout << "Any int copy/move " << count << ": "
              << elapsedMs(start, end) << " ms (sum=" << sum << ")\n";
}

int main()
{
    benchmarkBulkRegistration(100);
    benchmarkBatchRegistration(100);
    benchmarkQueries(100);
    benchmarkInvoke(100000);
    benchmarkAny(100000);
    return 0;
}
