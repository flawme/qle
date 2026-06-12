#!/bin/bash
echo "id,name,role,amount" > test.csv
echo "1,alice,ADMIN,100" >> test.csv
echo "2,bob,user,50" >> test.csv
echo "3,charlie,user,20" >> test.csv
echo "4,dave,ADMIN,200" >> test.csv

echo "Testing group by and sum:"
./build/qle "select role, sum(amount), count(id) from test.csv group by role"

echo ""
echo "Testing inline functions:"
./build/qle "select upper(name), length(role) from test.csv where lower(role) == 'admin'"
