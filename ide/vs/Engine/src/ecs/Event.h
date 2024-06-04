#pragma once
#include <functional>
#include <vector>

/// <summary>
/// The event template class that can be invoked with specific arguments
/// The class use std::function to store callbacks
/// </summary>
/// <typeparam name="...T"></typeparam>
template<typename ...T> class GCEvent
{
public:
    GCEvent() = default;
    ~GCEvent() = default;

    /// <summary>
    /// Adds a function to the event
    /// </summary>
    inline void operator += (const std::function<void(T...)>& function)
    {
        m_Functions.push_back(function);
    }
    /// <summary>
    /// Merges another event into this event
    /// </summary>
    inline void operator += (const GCEvent& event)
    {
        m_Functions.insert(m_Functions.end(), event.m_Functions.begin(), event.m_Functions.end());
    }
    /// <summary>
    /// Removes a function from the event
    /// </summary>
    inline void operator -= (const std::function<void(T...)>& function)
    {
        auto it = std::remove_if(m_Functions.begin(), m_Functions.end(),
            [&function](const std::function<void(T...)>& f) { return f.target_type() == function.target_type(); });
            m_Functions.erase(it, m_Functions.end());
    }
    /// <summary>
    /// Invokes the event
    /// </summary>
    void operator()(T... args)
    {
        for (auto& function : m_Functions)
        {
            if (function)
            {
                function(args...);
            }
        }
    }
    /// <summary>
    /// Invokes the event
    /// </summary>
    inline void Invoke(T... args)
    {
        operator()(args...);
    }
    /// <summary>
    /// Clears all functions from the event
    /// </summary>
    inline void Clear()
    {
        m_Functions.clear();
    }

private:
    std::vector<std::function<void(T...)>> m_Functions;
};
