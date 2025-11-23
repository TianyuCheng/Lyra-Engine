#ifndef LYRA_PLUGIN_D3D12_SIMPLE_HEAP_H
#define LYRA_PLUGIN_D3D12_SIMPLE_HEAP_H

#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Container.h>

using namespace lyra;

template <typename T>
struct Heap
{
    Vector<T> data = {};
    uint      tail = 0;

    uint allocate()
    {
        if (tail >= data.size())
            data.resize(data.size() * 2 + 1);
        return tail++;
    }

    void reset()
    {
        tail = 0;
    }

    void free()
    {
        tail = 0;
        data.clear();
    }

    T& at(uint i)
    {
        return data.at(i);
    }

    const T& at(uint i) const
    {
        return data.at(i);
    }
};

#endif // LYRA_PLUGIN_D3D12_SIMPLE_HEAP_H
