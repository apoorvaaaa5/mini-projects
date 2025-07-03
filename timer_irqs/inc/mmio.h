#pragma once
#define _MMIO_H_

#define mmio32(x)	(*(volatile unsigned int *) x)
