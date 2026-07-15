function play()
{
    CallHandler("play", "click", "");
}

function reset()
{
    CallHandler("reset", "click", "");
}

function setLines(lines)
{
    document.getElementById("lines").innerText = lines;
}

function setLevel(level)
{
    document.getElementById("level").innerText = level;
}

function setScore(score)
{
    document.getElementById("score").innerText = score;
}

function setSound(sound)
{
    document.getElementById("sound").checked = sound;
}

function sound()
{
    CallHandler("sound", "click", document.getElementById("sound").checked.toString());
}
