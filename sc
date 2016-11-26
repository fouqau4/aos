#!/bin/bash
ls -l test.c | sed 's/.\(..\).\(..\).\(..\)./\1\2\3/'
