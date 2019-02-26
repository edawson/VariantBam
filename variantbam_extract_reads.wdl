task ExtractReadsTask{
    File bam
    File bamIndex
    File rulesJSON
    String label
    Int threads
    Int diskGB

    String outbase = basename(bam, ".bam")



    command <<<
        variantbam ${bam} -r ${rulesJSON} -t ${threads} > ${outbase}.${label}.bam
    >>>


    runtime {
        docker : "erictdawson/variantbam"
        cpus : 4
        memory : "3.5 GB"
        preemptible_tries : 1
    }

    output{
        variantbam_extracted_reads : "${outbase}.${label}.bam"
    }
}

workflow variantbamExtractReads{
    File bam
    File bamIndex
    File rulesJSON
    String label
    Int threads

    Int diskGB = ceil(size(bam, "GB")) * 2

    call ExtractReadsTask{
        input:
            bam=bam,
            bamIndex=bamIndex,
            rulesJSON=rulesJSON,
            threads=threads,
            diskGB=diskGB,
            label=label
    }

}
