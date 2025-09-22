/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool* ContFramePool::pools[ContFramePool::MAX_POOLS] = {nullptr};
int ContFramePool::pool_count = 0;


ContFramePool::FrameState ContFramePool::get_state(unsigned long frame_no) {
    unsigned long rel = frame_no - base_frame_no;
    unsigned long bit = rel << 1;
    unsigned long byte_idx = bit >> 3;
    unsigned int  shift = bit & 7u;
    unsigned char slot = bitmap[byte_idx];
    unsigned char two_bits = (slot >> shift) & 0x03u;
    return static_cast<FrameState>(two_bits);
}

void ContFramePool::set_state(unsigned long frame_no, FrameState state) {
    unsigned long rel = frame_no - base_frame_no;
    unsigned long bit = rel << 1;
    unsigned long byte_idx = bit >> 3;
    unsigned int  shift = bit & 7u;
    unsigned char mask = static_cast<unsigned char>(0x03u << shift);
    unsigned char val  = static_cast<unsigned char>(static_cast<unsigned char>(state) << shift);
    unsigned char& slot = bitmap[byte_idx];
    slot = static_cast<unsigned char>((slot & ~mask) | val);
}



ContFramePool::ContFramePool(unsigned long base,
                             unsigned long count,
                             unsigned long info)
    : base_frame_no(base), n_frames(count), info_frame_no(info) {
    unsigned long bits = n_frames << 1;
    bitmap_size = (bits + 7) >> 3;

    if (info_frame_no == 0) {
        unsigned long info_frames = needed_info_frames(n_frames);
        bitmap = reinterpret_cast<unsigned char*>(base_frame_no * FRAME_SIZE);
        for (unsigned long i = 0; i < info_frames; ++i)
            set_state(base_frame_no + i, FrameState::Used);
        base_frame_no += info_frames;
        n_frames -= info_frames;
    } else {
        bitmap = reinterpret_cast<unsigned char*>(info_frame_no * FRAME_SIZE);
    }

    for (unsigned long i = 0; i < bitmap_size; ++i) bitmap[i] = 0;

    assert(pool_count < 2);
    pools[pool_count++] = this;
    Console::puts("Contiguous Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int n_req) {
    if (n_req == 0) return 0;
    unsigned long end = base_frame_no + n_frames;
    unsigned long run = 0, start = 0;
    for (unsigned long f = base_frame_no; f < end; ++f) {
        if (get_state(f) == FrameState::Free) {
            if (run == 0) start = f;
            if (++run == n_req) {
                set_state(start, FrameState::HoS);
                for (unsigned int i = 1; i < n_req; ++i)
                    set_state(start + i, FrameState::Used);
                return start;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long base, unsigned long count) {
    unsigned long end = base + count;
    for (unsigned long f = base; f < end; ++f)
        set_state(f, FrameState::Used);
}

void ContFramePool::release_frames(unsigned long first) {
    for (int i = 0; i < pool_count; ++i) {
        ContFramePool* p = pools[i];
        unsigned long begin = p->base_frame_no;
        unsigned long end = begin + p->n_frames;
        if (first >= begin && first < end) {
            if (p->get_state(first) != FrameState::HoS) {
                Console::puts("Error: Frame is not Head-of-Sequence\n");
                return;
            }
            p->set_state(first, FrameState::Free);
            unsigned long f = first + 1;
            while (f < end && p->get_state(f) == FrameState::Used) {
                p->set_state(f, FrameState::Free);
                ++f;
            }
            return;
        }
    }
    Console::puts("Error: Frame does not belong to any pool\n");
}

unsigned long ContFramePool::needed_info_frames(unsigned long n) {
    unsigned long bits = n << 1;
    unsigned long bytes = (bits + 7) >> 3;
    return (bytes + FRAME_SIZE - 1) / FRAME_SIZE;
}
