/*
 * Copyright (C) 2015 Niek Linnenbank
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Log.h>
#include <ListIterator.h>
#include <BitAllocator.h>
#include <BubbleAllocator.h>
#include <PoolAllocator.h>
#include "Kernel.h"
#include "Memory.h"
#include "Process.h"
#include "ProcessManager.h"
#include "Scheduler.h"
#include "API.h"

Kernel::Kernel(Address kernel, Size size, Size memorySize)
    : Singleton<Kernel>(this), m_interrupts(256)
{
    // Output log banners
    Log::instance->write(BANNER);
    Log::instance->write(COPYRIGHT "\r\n");

    // Initialize members
    m_memory = new BitAllocator(0, memorySize, PAGESIZE);
    m_procs  = new ProcessManager(new Scheduler());
    m_api    = new API();
    m_bootImageAddress = 0;

    // Mark kernel memory used
    for (Size i = 0; i < size; i += PAGESIZE)
        m_memory->allocate(kernel + i);

    // Clear interrupts table
    m_interrupts.fill(ZERO);
}

Error Kernel::heap(Address base, Size size)
{
    Allocator *bubble, *pool;
    Size meta = sizeof(BubbleAllocator) + sizeof(PoolAllocator);

    NOTICE("base = " << base << " size = " << size);

    // Setup the dynamic memory heap
    bubble = new (base) BubbleAllocator(base + meta, size - meta);
    pool   = new (base + sizeof(BubbleAllocator)) PoolAllocator();
    pool->setParent(bubble);

    // Set default allocator
    Allocator::setDefault(pool);
    return 0;
}

BitAllocator * Kernel::getMemory()
{
    return m_memory;
}

ProcessManager * Kernel::getProcessManager()
{
    return m_procs;
}

API * Kernel::getAPI()
{
    return m_api;
}

Address Kernel::getBootImageAddress()
{
    return m_bootImageAddress;
}

Size Kernel::getBootImageSize()
{
    return m_bootImageSize;
}

void Kernel::hookInterrupt(int vec, InterruptHandler h, ulong p)
{
    InterruptHook hook(h, p);

    // Insert into interrupts; create List if neccesary
    if (!m_interrupts[vec])
    {
        m_interrupts.insert(vec, new List<InterruptHook *>());
    }
    // Just append it. */
    if (!m_interrupts[vec]->contains(&hook))
    {
        m_interrupts[vec]->append(new InterruptHook(h, p));
    }
}

void Kernel::executeInterrupt(int vec, CPUState *state)
{
    /* Fetch the list of interrupt hooks (for this vector). */
    List<InterruptHook *> *lst = m_interrupts[vec];

    /* Does at least one handler exist? */
    if (!lst)
        return;

    /* Execute them all. */
    for (ListIterator<InterruptHook *> i(lst); i.hasCurrent(); i++)
    {
        i.current()->handler(state, i.current()->param);
    }
}

int Kernel::run()
{
    NOTICE("");

    // Load boot image programs
    loadBootImage();

    // Start the scheduler
    m_procs->schedule();

    // Never actually returns.
    return 0;
}
