export function secsToMins(duration_secs: number){
    const mins=Math.floor(duration_secs / 60)
    const secs=Math.round(duration_secs % 60)
    let minString="mins"
    if (mins==1) {minString="min"}
    let secString="secs"
    if (secs==1) {secString="sec"}
    return `${mins} ${minString}, ${secs} ${secString}`;
}