
#include <metal/drivers/.h>

/* These defines "redirect" the calls to the public Freedom Metal interrupt API
 * to the driver for the controller at compile time. Since they are the same
 * as the actual public API symbols, when they aren't defined (for instance,
 * if the Devicetree doesn't properly describe the interrupt parent for the device)
 * they will link to the stub functions in src/interrupt.c
 */

#define metal_interrupt_init(intc) _init((intc))
#define metal_interrupt_enable(intc, id) _enable((intc), (id))
#define metal_interrupt_disable(intc, id) _disable((intc), (id))