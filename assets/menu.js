function play()
{
    CallHandler("play", "click", "");
}

function reset()
{
    CallHandler("reset", "click", "");
}

function themeChanged()
{
    const theme = document.getElementById("theme").value;
    window.setThemePreference(theme);
    CallHandler("theme", "change", theme);
}

function setSaveVersionError(dataVersion, expectedVersion)
{
    document.getElementById("save-data-version").textContent = dataVersion;
    document.getElementById("save-expected-version").textContent = expectedVersion;
    document.getElementById("save-version-error").hidden = false;
    document.getElementById("play").disabled = true;
    ["action-sound", "step-sound", "show-controls", "theme"].forEach(function(id)
    {
        document.getElementById(id).disabled = true;
    });
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

function setQuadras(quadras)
{
    document.getElementById("quadras").innerText = quadras;
}

function setActionSound(sound)
{
    document.getElementById("action-sound").checked = sound;
}

function actionSound()
{
    CallHandler("action-sound", "click", document.getElementById("action-sound").checked.toString());
}

function setStepSound(sound)
{
    document.getElementById("step-sound").checked = sound;
}

function stepSound()
{
    CallHandler("step-sound", "click", document.getElementById("step-sound").checked.toString());
}

function setShowControls(show)
{
    document.getElementById("show-controls").checked = show;
}

function showControls()
{
    CallHandler("show-controls", "click", document.getElementById("show-controls").checked.toString());
}
