#include "Allocator.h"

Pointer::Pointer() : Pointer{ nullptr } {}

Pointer::Pointer(std::shared_ptr<void*> ptr) : ptr{ ptr } {}

Pointer::Pointer(const Pointer& src) : Pointer{ src.ptr } {}

void* Pointer::get() const
{
    if (ptr != nullptr)
    {
        return *ptr;
    }
    else
    {
        return nullptr;
    }
}

Allocator::Allocator(void *buffer, size_t buffer_size) : buffer_ptr{ buffer }, buffer_max_size{ buffer_size }
{
    free_blocks.emplace(buffer_ptr, buffer_size);
}


Pointer Allocator::alloc(size_t alloc_size)
{
    size_t free_block_size = SIZE_MAX;
    void* free_block_ptr = nullptr;
    for (auto block : free_blocks)
    {
        if (block.second >= alloc_size && block.second < free_block_size)
        {
            free_block_size = block.second;
            free_block_ptr = block.first;
        }
    }
    if (free_block_ptr == nullptr)
    {
        throw AllocError(AllocErrorType::NoMemory, "Out of memory");
    }
    else
    {
        size_t delta_size = free_block_size - alloc_size;
        free_blocks.erase(free_block_ptr);
        if (delta_size > 0)
        {
            free_blocks.emplace(static_cast<char*>(free_block_ptr) + alloc_size, delta_size);
        }
        std::shared_ptr<void*> addr(new void*);
        *addr = free_block_ptr;
        allocated_blocks.emplace(free_block_ptr, ptr_info(addr, alloc_size));
        return Pointer(addr);
    }
}

void Allocator::realloc(Pointer &p, size_t alloc_size)
{
    if (p.get() == NULL)
    {
        p = alloc(alloc_size);
        return;
    }
    
    // info about current block
    void* realloc_block = p.get();
    auto existing_block = allocated_blocks.find(realloc_block);
    if (existing_block == allocated_blocks.end())
    {
        throw AllocError(AllocErrorType::InvalidFree, "Invalid address for realloc");
    }
    ptr_info block_info = existing_block->second;
    size_t block_size = block_info.second;
    
    if (block_size == alloc_size)            // nothing changes
        
        return;
    
    else if (block_size > alloc_size)        // just shrink the block
    {
        size_t delta_size = block_size - alloc_size;
        block_info.second -= delta_size;
        free_blocks.emplace(static_cast<char*>(existing_block->first) + alloc_size, delta_size);
    }
    else
    {
        auto next_free_block = free_blocks.find(static_cast<char*>(existing_block->first) + existing_block->second.second);
        if (next_free_block != free_blocks.end() && (existing_block->second.second + next_free_block->second) >= alloc_size)
        {   // in place
            size_t delta_size = alloc_size - block_size;
            existing_block->second.second += delta_size;
            free_blocks.emplace(static_cast<char*>(next_free_block->first) + delta_size, next_free_block->second - delta_size);
            free_blocks.erase(next_free_block->first);
        }
        else
        {   // grow
            Pointer p_new = alloc(alloc_size);
            void* ptr_new_block = p_new.get();
            memmove(ptr_new_block, existing_block->first, existing_block->second.second);
            free(p);
            p = p_new;
        }
    }
}

void Allocator::free(Pointer &p)
{
    void* address_to_free = p.get();
    auto existing_block = allocated_blocks.find(address_to_free);
    if (existing_block == allocated_blocks.end())
    {
        return;
    }
    size_t existing_block_size = existing_block->second.second;
    
    for (auto current_block : free_blocks)
    {
        void* next_free_block_ptr = static_cast<char*>(existing_block->first) + existing_block_size;
        if (current_block.first == next_free_block_ptr)
        {
            void* new_free_block_ptr = existing_block->first;
            size_t new_free_block_size = current_block.second + existing_block_size;
            free_blocks.erase(current_block.first);
            
            free_blocks.emplace(new_free_block_ptr, new_free_block_size);
            free_allocated_block(existing_block);
            
            return;
        }
    }
    
    free_blocks.emplace(existing_block->first, existing_block_size);
    free_allocated_block(existing_block);
}

void Allocator::free_allocated_block(std::map<void*, ptr_info>::iterator existing_block)
{
    void* address_to_free = existing_block->first;
    
    *(existing_block->second.first) = nullptr;
    existing_block->second.second = 0;
    
    allocated_blocks.erase(address_to_free);
}

void Allocator::defrag()
{
    std::map<void*, ptr_info> new_heap;
    size_t heap_size = 0;
    char* current_ptr = static_cast<char*>(buffer_ptr);
    for (auto block : allocated_blocks)
    {
        ptr_info info = block.second;
        size_t block_size = info.second;
        
        memmove(current_ptr, *(info.first), block_size);
        *(info.first) = current_ptr;
        
        new_heap.emplace(current_ptr, ptr_info(info.first, info.second));
        current_ptr += block_size;
        heap_size += block_size;
    }
    allocated_blocks = new_heap;
    free_blocks.clear();
    if (heap_size < buffer_max_size)
    {
        free_blocks.emplace(static_cast<char*>(buffer_ptr) + heap_size, buffer_max_size - heap_size);
    }
}

std::string Allocator::dump()
{
    char msg_dump[128];
    std::string msg = "";
    
    for (auto block : allocated_blocks) {
        sprintf(msg_dump, "offset: %zu; ", block.first);
        msg += std::string(msg_dump);
        sprintf(msg_dump, "length: %zu; ", block.second.second);
        msg += std::string(msg_dump);
    }
    
    return msg;
}