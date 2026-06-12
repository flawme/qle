#!/bin/bash
echo "Testing avg, min, max:"
./build/qle "select role, avg(amount), min(amount), max(amount) from test.csv group by role"
