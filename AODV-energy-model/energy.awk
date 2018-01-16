#!/usr/bin/awk -f
BEGIN{
#print "energy of node 1"
data=0
}

{
if ($1=="N" && $5==0)
{
    print $3,$7
}
}
END{
#print "Completed"
}