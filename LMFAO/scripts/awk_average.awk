BEGIN {
    FS="\t"
    OFS=FS
};
{
    for(i=1; i<=NF; i++) {
        a[i]+=$i
        if($i!="") 
            b[i]++
    }
};
END {
    for(i=1; i<=NF; i++) 
        printf "%s%s", a[i]/b[i], (i==NF?ORS:OFS)
}
