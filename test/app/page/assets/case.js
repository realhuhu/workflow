(function () {
    "use strict";

    var slug = document.body.getAttribute("data-case");
    var definition = (window.WORKFLOW_CASES || []).find(function (item) {
        return item.slug === slug;
    });
    var root = document.getElementById("case-root");
    var timers = [];
    var cleanups = [];
    var state = {};
    var markerRoot = "../游戏图片/fixtures/";

    if (!definition) {
        root.textContent = "Unknown test page: " + slug;
        return;
    }

    function number() {
        return String(definition.id).padStart(2, "0");
    }

    function buildShell() {
        root.innerHTML =
            "<header class=\"case-header\">" +
                "<div class=\"case-id\">" + number() + "</div>" +
                "<div class=\"case-copy\"><span class=\"overline\">" + definition.group + " / DETERMINISTIC FIXTURE</span>" +
                    "<h1>" + definition.title + "</h1><p>" + definition.summary + "</p></div>" +
                "<div class=\"case-meta\"><code>" + definition.api + "</code>" +
                    "<span class=\"case-result\" id=\"case-result\" data-state=\"pending\">PENDING</span></div>" +
            "</header>" +
            "<main class=\"fixture-stage\" id=\"fixture-stage\"><span class=\"stage-label\">CASE " + number() + " / TEST FIELD</span></main>" +
            "<div class=\"environment-blocker\" id=\"environment-blocker\"><div><strong>DPI / ZOOM 校准失败</strong>" +
                "<code id=\"environment-detail\"></code></div></div>";
    }

    function stage() {
        return document.getElementById("fixture-stage");
    }

    function later(callback, delay) {
        var timer = window.setTimeout(callback, delay);
        timers.push(timer);
        return timer;
    }

    function every(callback, delay) {
        var timer = window.setInterval(callback, delay);
        timers.push(timer);
        return timer;
    }

    function cleanup() {
        timers.forEach(function (timer) {
            window.clearTimeout(timer);
            window.clearInterval(timer);
        });
        timers = [];
        cleanups.forEach(function (callback) { callback(); });
        cleanups = [];
    }

    function position(element, x, y) {
        element.style.left = x + "px";
        element.style.top = y + "px";
        return element;
    }

    function marker(name, x, y, onClick, extraClass) {
        var image = document.createElement("img");
        image.className = "fixture-marker" + (onClick ? " clickable" : "") + (extraClass ? " " + extraClass : "");
        image.src = markerRoot + name + ".png";
        image.alt = name;
        image.draggable = false;
        image.setAttribute("data-target", name);
        position(image, x, y);
        if (onClick) image.addEventListener("click", onClick);
        stage().appendChild(image);
        return image;
    }

    function textTarget(text, x, y, width, onClick) {
        var element = document.createElement("div");
        element.className = "ocr-target" + (onClick ? " clickable" : "");
        element.textContent = text;
        element.style.width = width + "px";
        element.setAttribute("data-target", text);
        position(element, x, y);
        if (onClick) element.addEventListener("click", onClick);
        stage().appendChild(element);
        return element;
    }

    function roi(x, y, width, height) {
        var element = document.createElement("div");
        element.className = "roi-box";
        element.style.width = width + "px";
        element.style.height = height + "px";
        position(element, x, y);
        stage().appendChild(element);
        return element;
    }

    function completion(text, x, y) {
        var element = document.createElement("div");
        element.className = "completion";
        element.textContent = text;
        element.setAttribute("data-target", text);
        position(element, x == null ? 330 : x, y == null ? 500 : y);
        stage().appendChild(element);
        return element;
    }

    function setResult(result, label) {
        document.body.dataset.result = result;
        state.result = result;
        state.label = label || result;
        var badge = document.getElementById("case-result");
        badge.dataset.state = result;
        badge.textContent = (label || result).toUpperCase();
    }

    function pass(label) {
        setResult("pass", label || "PASS");
    }

    function fail(label) {
        setResult("fail", label || "WRONG TARGET");
    }

    function environment() {
        var result = window.workflowEnvironment();
        var allowUnsafe = new URLSearchParams(window.location.search).get("allowUnsafe") === "1";
        document.documentElement.dataset.environment = result.ok || allowUnsafe ? "valid" : "invalid";
        document.getElementById("environment-detail").textContent =
            "DPR=" + result.dpr + " · SCALE=" + result.scale + " · VIEWPORT=" + result.width + "×" + result.height;
        return result;
    }

    var renderers = {
        "static-image": function () {
            marker("marker-a", 430, 270, function (event) {
                event.currentTarget.remove();
                marker("marker-b", 760, 520);
                pass("IMAGE CLICKED");
            });
        },

        "image-selector": function () {
            [[110, 180, "left"], [420, 400, "middle"], [760, 180, "right"]].forEach(function (item) {
                marker("marker-a", item[0], item[1], function () {
                    if (item[2] !== "right") return fail("WRONG SLOT");
                    marker("marker-c", 760, 510);
                    pass("RIGHT-MOST");
                });
            });
        },

        "any-image": function () {
            var starter = marker("marker-a", 430, 270, function () {
                starter.remove();
                marker("marker-b", 220, 300, function () { fail("B SELECTED"); });
                marker("marker-c", 690, 300, function () {
                    stage().querySelectorAll(".fixture-marker").forEach(function (node) { node.remove(); });
                    marker("marker-a", 430, 500);
                    pass("C SELECTED");
                });
            });
        },

        "delayed-image": function () {
            state.available = false;
            marker("marker-b", 100, 520, function () {
                if (!state.available) return fail("TOO EARLY");
                marker("marker-c", 760, 510);
                pass("DELAY PASS");
            }, "control-marker");
            later(function () {
                marker("marker-a", 440, 260);
                state.available = true;
            }, 1200);
        },

        "disappearing-image": function () {
            state.absent = false;
            var transient = marker("marker-a", 440, 250);
            marker("marker-b", 100, 520, function () {
                if (!state.absent) return fail("STILL VISIBLE");
                marker("marker-c", 760, 510);
                pass("ABSENT");
            }, "control-marker");
            later(function () {
                transient.remove();
                state.absent = true;
            }, 1200);
        },

        "stable-image": function () {
            state.stable = false;
            var moving = marker("marker-a", 100, 180);
            var positions = [[700, 160], [220, 340], [650, 410], [350, 170], [440, 280]];
            var index = 0;
            var timer = every(function () {
                position(moving, positions[index][0], positions[index][1]);
                index += 1;
                if (index < positions.length) return;
                window.clearInterval(timer);
                state.stable = true;
            }, 260);
            marker("marker-b", 100, 540, function () {
                if (!state.stable) return fail("MOVING");
                marker("marker-c", 760, 520);
                pass("STABLE");
            }, "control-marker");
        },

        "image-region": function () {
            roi(540, 130, 370, 380);
            marker("marker-a", 120, 280, function () { fail("OUTSIDE ROI"); });
            marker("marker-a", 700, 280, function () {
                marker("marker-c", 760, 520);
                pass("INSIDE ROI");
            });
        },

        "image-threshold": function () {
            marker("marker-a-variant", 180, 280, function () { fail("VARIANT"); });
            marker("marker-a", 700, 280, function () {
                marker("marker-c", 760, 520);
                pass("EXACT MATCH");
            });
        },

        "static-text": function () {
            textTarget("CONFIRM", 340, 270, 280, function (event) {
                event.currentTarget.remove();
                completion("TEXT CLICKED");
                pass("TEXT CLICKED");
            });
        },

        "any-text": function () {
            var starter = marker("marker-a", 430, 270, function () {
                starter.remove();
                textTarget("CONTINUE", 130, 290, 300, function () { fail("CONTINUE"); });
                textTarget("CONFIRM", 560, 290, 270, function () {
                    completion("PRIORITY PASS");
                    pass("CONFIRM FIRST");
                });
            });
        },

        "delayed-text": function () {
            state.ready = false;
            marker("marker-b", 100, 520, function () {
                if (!state.ready) return fail("TOO EARLY");
                completion("DELAY PASS");
                pass("READY");
            }, "control-marker");
            later(function () {
                textTarget("READY", 350, 250, 260);
                state.ready = true;
            }, 1200);
        },

        "disappearing-text": function () {
            state.absent = false;
            var transient = textTarget("PROCESSING", 300, 250, 360);
            marker("marker-b", 100, 520, function () {
                if (!state.absent) return fail("STILL PRESENT");
                completion("ABSENCE PASS");
                pass("ABSENT");
            }, "control-marker");
            later(function () {
                transient.remove();
                state.absent = true;
            }, 1200);
        },

        "stable-text": function () {
            state.stable = false;
            var moving = textTarget("STEADY", 80, 220, 250);
            var positions = [620, 240, 540, 160, 360];
            var index = 0;
            var timer = every(function () {
                moving.style.left = positions[index] + "px";
                index += 1;
                if (index < positions.length) return;
                window.clearInterval(timer);
                state.stable = true;
            }, 260);
            marker("marker-b", 100, 540, function () {
                if (!state.stable) return fail("MOVING");
                completion("STABLE PASS");
                pass("STABLE");
            }, "control-marker");
        },

        "text-region": function () {
            roi(500, 130, 390, 380);
            textTarget("CONFIRM", 80, 280, 260, function () { fail("OUTSIDE ROI"); });
            textTarget("CONFIRM", 600, 280, 260, function () {
                completion("ROI PASS");
                pass("INSIDE ROI");
            });
        },

        "previous-relation": function () {
            marker("marker-a", 420, 280);
            textTarget("LEFT TARGET", 80, 276, 280, function () { fail("LEFT"); });
            textTarget("RIGHT TARGET", 560, 276, 300, function () {
                completion("RELATION PASS");
                pass("RIGHT CENTER");
            });
        },

        "repeat-click": function () {
            state.clicks = 0;
            var counter = document.createElement("div");
            counter.className = "counter";
            position(counter, 420, 350);
            stage().appendChild(counter);
            function update() { counter.textContent = "CLICK COUNT / " + state.clicks; }
            update();
            marker("marker-a", 450, 240, function () {
                state.clicks += 1;
                update();
                if (state.clicks < 3) return;
                completion("CLICK COMPLETE");
                pass("3 CLICKS");
            });
        },

        "drag": function () {
            var zone = document.createElement("div");
            zone.className = "drop-zone";
            zone.textContent = "DROP ZONE";
            zone.style.width = "200px";
            zone.style.height = "120px";
            position(zone, 374, 430);
            stage().appendChild(zone);

            var piece = marker("marker-a", 450, 80, null, "clickable");
            var dragging = false;
            var startY = 0;
            var originTop = 0;
            function down(event) {
                event.preventDefault();
                dragging = true;
                startY = event.clientY;
                originTop = parseInt(piece.style.top, 10);
            }
            function move(event) {
                if (!dragging) return;
                var next = Math.max(60, Math.min(540, originTop + event.clientY - startY));
                piece.style.top = next + "px";
            }
            function up() {
                if (!dragging) return;
                dragging = false;
                var top = parseInt(piece.style.top, 10);
                if (top + 24 < 430 || top + 24 > 550) return;
                completion("DRAG COMPLETE", 330, 300);
                pass("DROPPED");
            }
            piece.addEventListener("mousedown", down);
            document.addEventListener("mousemove", move);
            document.addEventListener("mouseup", up);
            cleanups.push(function () {
                document.removeEventListener("mousemove", move);
                document.removeEventListener("mouseup", up);
            });
        },

        "scroll": function () {
            var viewport = document.createElement("div");
            viewport.className = "scroll-viewport";
            viewport.style.left = "158px";
            viewport.style.top = "70px";
            viewport.style.width = "640px";
            viewport.style.height = "500px";
            var content = document.createElement("div");
            content.className = "scroll-content";
            var anchor = document.createElement("img");
            anchor.className = "fixture-marker scroll-anchor";
            anchor.src = markerRoot + "marker-a.png";
            anchor.alt = "marker-a";
            anchor.draggable = false;
            anchor.setAttribute("data-target", "marker-a");
            var finish = document.createElement("div");
            finish.className = "completion scroll-finish";
            finish.textContent = "SCROLL COMPLETE";
            finish.setAttribute("data-target", "SCROLL COMPLETE");
            content.appendChild(anchor);
            content.appendChild(finish);
            viewport.appendChild(content);
            stage().appendChild(viewport);
            viewport.addEventListener("scroll", function () {
                if (viewport.scrollTop + viewport.clientHeight < viewport.scrollHeight - 24) return;
                pass("BOTTOM REACHED");
            });
        },

        "hidden-layer": function () {
            marker("marker-b", 100, 520, null, "control-marker");
            marker("marker-a", 430, 250, function () {
                marker("marker-c", 760, 510);
                pass("LAYER CLEARED");
            });
            var overlay = document.createElement("div");
            overlay.className = "overlay-panel";
            overlay.textContent = "OCCLUSION LAYER";
            overlay.style.width = "260px";
            overlay.style.height = "220px";
            position(overlay, 350, 180);
            stage().appendChild(overlay);
            later(function () { overlay.remove(); }, 1200);
        },

        "mixed-workflow": function () {
            var first = marker("marker-a", 440, 270, function () {
                first.remove();
                var next = textTarget("CONTINUE", 330, 270, 300, function () {
                    next.remove();
                    marker("marker-c", 450, 270, function (event) {
                        event.currentTarget.remove();
                        completion("WORKFLOW COMPLETE");
                        pass("CHAIN COMPLETE");
                    });
                });
            });
        }
    };

    function reset() {
        cleanup();
        state = { result: "pending" };
        document.body.dataset.result = "pending";
        buildShell();
        setResult("pending", "PENDING");
        renderers[slug]();
        environment();
        return true;
    }

    window.workflowFixture = {
        environment: environment,
        reset: reset,
        state: function () {
            return JSON.parse(JSON.stringify(state));
        }
    };

    window.addEventListener("resize", environment);
    window.addEventListener("keydown", function (event) {
        if (event.key === "r" || event.key === "R") reset();
    });
    reset();
})();
