(function () {
    "use strict";

    window.WORKFLOW_CASES = [
        { id: 1, slug: "static-image", title: "静态图片点击", group: "IMAGE", api: "ImageClicker::click", summary: "定位并点击唯一像素模板。" },
        { id: 2, slug: "image-selector", title: "多目标位置选择", group: "IMAGE", api: "positionSelector", summary: "同图多目标时选择最右侧结果。" },
        { id: 3, slug: "any-image", title: "AnyImage 候选优先级", group: "IMAGE", api: "AnyImage", summary: "共享截图并按候选顺序命中。" },
        { id: 4, slug: "delayed-image", title: "延迟出现图片", group: "UNTIL", api: "Image", summary: "等待 1200 ms 后出现的模板。" },
        { id: 5, slug: "disappearing-image", title: "等待图片消失", group: "UNTIL", api: "Image(reverse)", summary: "有效截图未命中后继续流程。" },
        { id: 6, slug: "stable-image", title: "图片稳定检测", group: "UNTIL", api: "ImageStable", summary: "连续三次中心位置保持一致。" },
        { id: 7, slug: "image-region", title: "图片 ROI 裁剪", group: "IMAGE", api: "ImageInitConfig::region", summary: "只搜索标定区域内的模板。" },
        { id: 8, slug: "image-threshold", title: "图片相似度阈值", group: "IMAGE", api: "threshold", summary: "高阈值排除近似模板。" },
        { id: 9, slug: "static-text", title: "静态文字点击", group: "TEXT", api: "TextClicker::click", summary: "OCR 定位并点击高对比文字。" },
        { id: 10, slug: "any-text", title: "AnyText 候选优先级", group: "TEXT", api: "AnyText", summary: "一次 OCR 后按候选顺序选择。" },
        { id: 11, slug: "delayed-text", title: "延迟出现文字", group: "UNTIL", api: "Text", summary: "等待动态文字进入 OCR 区域。" },
        { id: 12, slug: "disappearing-text", title: "等待文字消失", group: "UNTIL", api: "Text(reverse)", summary: "文字消失后继续执行动作。" },
        { id: 13, slug: "stable-text", title: "文字稳定检测", group: "UNTIL", api: "TextStable", summary: "文字框连续三次位置不变。" },
        { id: 14, slug: "text-region", title: "OCR 推理前裁剪", group: "TEXT", api: "TextInitConfig::region", summary: "裁剪 ROI 后再执行 OCR。" },
        { id: 15, slug: "previous-relation", title: "Previous 空间关系", group: "MIXED", api: "RIGHT_CENTER", summary: "从上一步 Segment 派生文字 ROI。" },
        { id: 16, slug: "repeat-click", title: "循环点击直到满足", group: "ACTION", api: "click + runUntil", summary: "第三次点击后出现终止条件。" },
        { id: 17, slug: "drag", title: "纵向拖动", group: "ACTION", api: "drag", summary: "拖动并重新匹配，直到进入投放区。" },
        { id: 18, slug: "scroll", title: "滚轮滚动", group: "ACTION", api: "scroll + Text", summary: "当前页未满足时才发送滚轮。" },
        { id: 19, slug: "hidden-layer", title: "遮挡层消失", group: "UNTIL", api: "Image + overlay", summary: "遮挡移除后模板才可命中。" },
        { id: 20, slug: "mixed-workflow", title: "图片文字混合链", group: "MIXED", api: "Image → Text → Image", summary: "验证跨匹配类型的原始链式 API。" }
    ];

    window.workflowEnvironment = function () {
        var viewport = window.visualViewport;
        var scale = viewport ? viewport.scale : 1;
        var dpr = window.devicePixelRatio || 1;
        var width = window.innerWidth;
        var height = window.innerHeight;
        var ok = Math.abs(dpr - 1) < 0.001 &&
            Math.abs(scale - 1) < 0.001 &&
            width === 1000 &&
            height === 800;
        return {
            ok: ok,
            dpr: dpr,
            scale: scale,
            width: width,
            height: height
        };
    };
})();
