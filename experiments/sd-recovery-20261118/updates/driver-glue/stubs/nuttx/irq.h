typedef unsigned irqstate_t;
irqstate_t enter_critical_section(void);
void leave_critical_section(irqstate_t flags);
