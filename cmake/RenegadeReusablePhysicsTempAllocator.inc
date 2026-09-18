namespace
{
    // Recycle large transient Jolt buffers to avoid repeated allocation churn.
    // Retains the original 65,536-contact capacity and malloc fallback.
    class ReusablePhysicsTempAllocator final : public TempAllocator
    {
        struct Block { void* data = nullptr; uint size = 0; };
        static constexpr uint kMinCachedSize = 1024u * 1024u;
        static constexpr std::uint64_t kMaxCachedBytes = 192ull * 1024ull * 1024ull;
        TempAllocatorMalloc fallback_;
        std::mutex mutex_;
        std::array<Block, 4> cache_{};
        std::uint64_t cached_bytes_ = 0;
    public:
        ~ReusablePhysicsTempAllocator() override
        {
            for (auto& block : cache_)
                if (block.data != nullptr) fallback_.Free(block.data, block.size);
        }
        void* Allocate(uint size) override
        {
            if (size >= kMinCachedSize)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& block : cache_)
                    if (block.data != nullptr && block.size == size)
                    {
                        void* result = block.data;
                        block = {};
                        cached_bytes_ -= size;
                        return result;
                    }
            }
            return fallback_.Allocate(size);
        }
        void Free(void* address, uint size) override
        {
            if (address != nullptr && size >= kMinCachedSize && size <= kMaxCachedBytes)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (cached_bytes_ + size <= kMaxCachedBytes)
                    for (auto& block : cache_)
                        if (block.data == nullptr)
                        {
                            block = {address, size};
                            cached_bytes_ += size;
                            return;
                        }
            }
            fallback_.Free(address, size);
        }
    };
}
