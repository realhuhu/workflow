#include "cases.h"

#include "workflow.h"

#include <QMargins>
#include <QRect>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

    const QString MarkerA = QStringLiteral("fixtures/marker-a.png");
    const QString MarkerB = QStringLiteral("fixtures/marker-b.png");
    const QString MarkerC = QStringLiteral("fixtures/marker-c.png");
    const QRect StageRegion(20, 120, 960, 660);

    void runStaticImage() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Image(
                                MarkerB,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runImageSelector() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .selector = positionSelector(SelectorBasis::X_CENTER, SelectorMethod::MAX),
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runAnyImage() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new AnyImage(
                                std::vector<QString>{MarkerC, MarkerB},
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Image(
                                MarkerA,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runDelayedImage() {
        std::make_unique<ImageClicker>(
            MarkerB,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .startUntilList =
                        {
                            new Image(
                                MarkerA,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runDisappearingImage() {
        std::make_unique<ImageClicker>(
            MarkerB,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .startUntilList =
                        {
                            new Image(
                                MarkerA,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .reverse = true,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runStableImage() {
        std::make_unique<ImageClicker>(
            MarkerB,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .startUntilList =
                        {
                            new ImageStable(
                                MarkerA,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runImageRegion() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = QRect(560, 250, 370, 380),
            }
        )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runImageThreshold() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.995f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runStaticText() {
        std::make_unique<TextClicker>(
            QStringLiteral("CONFIRM"),
            TextInitConfig{
                .timeout = 12,
                .region = StageRegion,
                .match =
                    {
                        .threshold = 0.25f,
                        .boxThreshold = 0.25f,
                    },
            }
        )
            ->click(
                TextRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("TEXT CLICKED"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runAnyText() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new AnyText(
                                std::vector<QString>{QStringLiteral("CONFIRM"), QStringLiteral("CONTINUE")},
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->click(
                TextRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("PRIORITY PASS"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runDelayedText() {
        std::make_unique<ImageClicker>(
            MarkerB,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .startUntilList =
                        {
                            new Text(
                                QStringLiteral("READY"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("DELAY PASS"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runDisappearingText() {
        std::make_unique<ImageClicker>(
            MarkerB,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .startUntilList =
                        {
                            new Text(
                                QStringLiteral("PROCESSING"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .reverse = true,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("ABSENCE PASS"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runStableText() {
        std::make_unique<ImageClicker>(
            MarkerB,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .startUntilList =
                        {
                            new TextStable(
                                QStringLiteral("STEADY"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("STABLE PASS"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runTextRegion() {
        std::make_unique<TextClicker>(
            QStringLiteral("CONFIRM"),
            TextInitConfig{
                .timeout = 12,
                .region = QRect(520, 250, 390, 380),
                .match =
                    {
                        .threshold = 0.25f,
                        .boxThreshold = 0.25f,
                    },
            }
        )
            ->click(
                TextRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("ROI PASS"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runPreviousRelation() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->locate(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("RIGHT TARGET"),
                                TextUntilConfig{
                                    .onPrevious = Previous::RIGHT_CENTER,
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                    .cropPadding = QMargins(0, 18, 0, 18),
                                }
                            ),
                        },
                    .homing = false,
                }
            )
            ->click(
                TextRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("RELATION PASS"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runRepeatClick() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .runUntilList =
                        {
                            new Text(
                                QStringLiteral("CLICK COMPLETE"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.12f
            )
            ->end();
    }

    void runDrag() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->drag(
                ImageRunConfig{
                    .runUntilList =
                        {
                            new Text(
                                QStringLiteral("DRAG COMPLETE"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                120
            )
            ->end();
    }

    void runScroll() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->scroll(
                ImageRunConfig{
                    .runUntilList =
                        {
                            new Text(
                                QStringLiteral("SCROLL COMPLETE"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = QRect(170, 180, 660, 530),
                                }
                            ),
                        },
                    .homing = false,
                },
                -WheelDelta * 4,
                0.15f
            )
            ->end();
    }

    void runHiddenLayer() {
        std::make_unique<ImageClicker>(
            MarkerB,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->locate(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Image(
                                MarkerA,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                }
            )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

    void runMixedWorkflow() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("CONTINUE"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->click(
                TextRunConfig{
                    .finishUntilList =
                        {
                            new Image(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("WORKFLOW COMPLETE"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
                    .homing = false,
                },
                0.1f
            )
            ->end();
    }

} // namespace

const std::vector<BrowserCase>& browserCases() {
    static const std::vector<BrowserCase> cases{
        {1,
            "static-image",
            "静态图片点击",
            "IMAGE",
            "ImageClicker::click",
            "marker-a.png",
            "marker-b.png",
            "定位并点击唯一的像素模板。"},
        {2,
            "image-selector",
            "多目标位置选择",
            "IMAGE",
            "positionSelector",
            "3 × marker-a.png",
            "right-most",
            "同图多目标时选择最右侧结果。"},
        {3,
            "any-image",
            "AnyImage 候选优先级",
            "IMAGE",
            "AnyImage",
            "marker-c | marker-b",
            "marker-c",
            "共享截图并按候选顺序选择目标。"},
        {4,
            "delayed-image",
            "延迟出现图片",
            "UNTIL",
            "Image",
            "marker-a.png",
            "appears after 1200 ms",
            "等待延迟目标出现后再执行点击。"},
        {5,
            "disappearing-image",
            "等待图片消失",
            "UNTIL",
            "Image(reverse)",
            "marker-a.png",
            "absent",
            "反向条件只在有效截图的未命中时满足。"},
        {6,
            "stable-image",
            "图片稳定检测",
            "UNTIL",
            "ImageStable",
            "moving marker-a.png",
            "3 stable samples",
            "目标停止移动并连续三次中心一致。"},
        {7,
            "image-region",
            "图片 ROI 裁剪",
            "IMAGE",
            "ImageInitConfig::region",
            "2 × marker-a.png",
            "inside ROI",
            "仅在指定截图区域内执行模板匹配。"},
        {8,
            "image-threshold",
            "图片相似度阈值",
            "IMAGE",
            "threshold",
            "exact + variant",
            "exact only",
            "高阈值排除近似但不相同的模板。"},
        {9,
            "static-text",
            "静态文字点击",
            "TEXT",
            "TextClicker::click",
            "CONFIRM",
            "TEXT CLICKED",
            "OCR 定位高对比文字并点击。"},
        {10,
            "any-text",
            "AnyText 候选优先级",
            "TEXT",
            "AnyText",
            "CONFIRM | CONTINUE",
            "CONFIRM",
            "一次 OCR 后按候选顺序选择文字。"},
        {11, "delayed-text", "延迟出现文字", "UNTIL", "Text", "READY", "DELAY PASS", "在 ROI 内等待动态文字出现。"},
        {12,
            "disappearing-text",
            "等待文字消失",
            "UNTIL",
            "Text(reverse)",
            "PROCESSING",
            "ABSENCE PASS",
            "文字消失后继续执行动作。"},
        {13,
            "stable-text",
            "文字稳定检测",
            "UNTIL",
            "TextStable",
            "STEADY",
            "STABLE PASS",
            "OCR 文字框连续三次保持同一位置。"},
        {14,
            "text-region",
            "OCR 推理前裁剪",
            "TEXT",
            "TextInitConfig::region",
            "2 × CONFIRM",
            "inside ROI",
            "裁剪后再 OCR，避免识别整个窗口。"},
        {15,
            "previous-relation",
            "Previous 空间关系",
            "MIXED",
            "RIGHT_CENTER",
            "marker-a → RIGHT TARGET",
            "RELATION PASS",
            "由上一步 Segment 派生 OCR 裁剪区域。"},
        {16,
            "repeat-click",
            "循环点击直到满足",
            "ACTION",
            "click + runUntil",
            "marker-a × 3",
            "CLICK COMPLETE",
            "重复点击并以文字条件终止循环。"},
        {17, "drag", "纵向拖动", "ACTION", "drag", "marker-a", "DRAG COMPLETE", "拖动后重新匹配目标，直到进入投放区。"},
        {18,
            "scroll",
            "滚轮滚动",
            "ACTION",
            "scroll + Text",
            "sticky marker-a",
            "SCROLL COMPLETE",
            "先检查当前页，未满足时再发送滚轮。"},
        {19,
            "hidden-layer",
            "遮挡层消失",
            "UNTIL",
            "Image + overlay",
            "covered marker-a",
            "visible and clicked",
            "遮挡移除后才允许模板命中。"},
        {20,
            "mixed-workflow",
            "图片文字混合链",
            "MIXED",
            "Image → Text → Image",
            "A → CONTINUE → C",
            "WORKFLOW COMPLETE",
            "验证原始链式 API 跨匹配类型传播。"},
    };
    return cases;
}

void runBrowserCase(
    const int id
) {
    switch (id) {
        case 1:
            return runStaticImage();
        case 2:
            return runImageSelector();
        case 3:
            return runAnyImage();
        case 4:
            return runDelayedImage();
        case 5:
            return runDisappearingImage();
        case 6:
            return runStableImage();
        case 7:
            return runImageRegion();
        case 8:
            return runImageThreshold();
        case 9:
            return runStaticText();
        case 10:
            return runAnyText();
        case 11:
            return runDelayedText();
        case 12:
            return runDisappearingText();
        case 13:
            return runStableText();
        case 14:
            return runTextRegion();
        case 15:
            return runPreviousRelation();
        case 16:
            return runRepeatClick();
        case 17:
            return runDrag();
        case 18:
            return runScroll();
        case 19:
            return runHiddenLayer();
        case 20:
            return runMixedWorkflow();
        default:
            throw std::out_of_range("未知浏览器测试用例: " + std::to_string(id));
    }
}
