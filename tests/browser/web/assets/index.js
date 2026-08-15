(function () {
    "use strict";

    var root = document.getElementById("index-root");
    var cases = window.WORKFLOW_CASES || [];
    var cards = cases.map(function (item) {
        var number = String(item.id).padStart(2, "0");
        return "<a class=\"case-card\" href=\"cases/" + number + "-" + item.slug + ".html\">" +
            "<span class=\"case-number\">" + number + "</span>" +
            "<span class=\"case-group\">" + item.group + "</span>" +
            "<strong>" + item.title + "</strong>" +
            "<code>" + item.api + "</code>" +
            "<small>" + item.summary + "</small>" +
            "</a>";
    }).join("");

    root.innerHTML =
        "<header class=\"index-header\">" +
            "<div><span class=\"overline\">WORKFLOW / BROWSER FIXTURES</span>" +
            "<h1>自动化校准矩阵</h1></div>" +
            "<div class=\"calibration-chip\" id=\"environment-chip\">CHECKING</div>" +
        "</header>" +
        "<section class=\"index-intro\">" +
            "<p>20 个离线二级页面，覆盖图片、OCR、Until、点击、拖动、滚动和混合链。</p>" +
            "<span>固定画布 1000 × 800 · DPR 1 · Zoom 100%</span>" +
        "</section>" +
        "<main class=\"case-grid\">" + cards + "</main>";

    function environment() {
        var result = window.workflowEnvironment();
        var chip = document.getElementById("environment-chip");
        chip.textContent = result.ok ? "CALIBRATED" : "DPI / ZOOM ERROR";
        chip.setAttribute("data-ok", result.ok ? "true" : "false");
        return result;
    }

    window.workflowFixture = {
        environment: environment,
        reset: function () { return true; },
        state: function () { return { result: "index" }; }
    };
    environment();
})();
