#!/bin/bash

# By zeroing out Tag_ABI_PCS_wchar_t, we indicate that
# this ELF file is wchar_t-agnostic.

$(dirname $0)/arm-wchar-tag $1 0
