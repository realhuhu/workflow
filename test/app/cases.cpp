#include "cases.h"

#include "workflow.h"

#include <QDir>
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
                    .finishWait = 0.5f,
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
                                QStringLiteral("CONFIRM"),
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
                0.5f
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

    void runOptionalUntils() {
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
                    .startUntilList =
                        {
                            new IfImage(
                                MarkerC,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .region = StageRegion,
                                }
                            ),
                            new IfAnyImage(
                                std::vector<QString>{MarkerC, MarkerB},
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .region = StageRegion,
                                }
                            ),
                            new IfText(
                                QStringLiteral("OPTIONAL TEXT"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                            new IfAnyText(
                                std::vector<QString>{QStringLiteral("OPTIONAL A"), QStringLiteral("OPTIONAL B")},
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .boxThreshold = 0.25f,
                                    .region = StageRegion,
                                }
                            ),
                        },
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

    void runTextMatchModes() {
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
                                QStringLiteral("ORDER-[0-9]{4}"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .match = TextMatch::REGEX,
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
                                QStringLiteral("CONFIRN"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .match = TextMatch::FUZZY,
                                    .boxThreshold = 0.25f,
                                    .maxEditDistance = 1,
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
                                QStringLiteral("MATCH MODES PASS"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .match = TextMatch::EXACT,
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

    void runOrderedSelector() {
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
                    .selector = orderedRandomSelector(SelectorBasis::X_CENTER, SelectorMethod::MAX, 2),
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

    void runClickAnchorOffset() {
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
                0.1f,
                12,
                0,
                Click::LEFT
            )
            ->end();
    }

    void runReverseDrag() {
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
                                QStringLiteral("REVERSE DRAG COMPLETE"),
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
                120,
                true
            )
            ->end();
    }

    void runUpwardScroll() {
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
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("SCROLL UP COMPLETE"),
                                TextUntilConfig{
                                    .threshold = 0.25f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .boxThreshold = 0.25f,
                                    .region = QRect(170, 180, 660, 530),
                                }
                            ),
                        },
                    .finishWait = 0.5f,
                    .homing = false,
                },
                WheelDelta * 100,
                0.15f
            )
            ->end();
    }

    void runMultipleUntilAnd() {
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
                            new Image(
                                MarkerB,
                                ImageUntilConfig{
                                    .threshold = 0.98f,
                                    .interval = 0.12f,
                                    .timeout = 8,
                                    .region = StageRegion,
                                }
                            ),
                            new Text(
                                QStringLiteral("BOTH READY"),
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

    void runWaitPhases() {
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
                    .startWait = 0.6f,
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
                    .finishWait = 0.6f,
                    .homing = true,
                },
                0.1f
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

    void runBranchIfNone() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->branch(
                std::make_unique<IfImage>(
                    MarkerC,
                    ImageUntilConfig{
                        .threshold = 0.98f,
                        .region = StageRegion,
                    }
                ),
                BranchMap{
                    {MarkerC, [](std::unique_ptr<ClickerBase> current) { return current; }},
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

    void runBranchAnyImage() {
        std::make_unique<ImageClicker>(
            MarkerC,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->branch(
                std::make_unique<AnyImage>(
                    std::vector<QString>{MarkerC, MarkerB},
                    ImageUntilConfig{
                        .threshold = 0.98f,
                        .interval = 0.12f,
                        .timeout = 8,
                        .region = StageRegion,
                    }
                ),
                BranchMap{
                    {MarkerC,
                        [](std::unique_ptr<ClickerBase> current) {
                            return current->click(
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
                            );
                        }},
                    {MarkerB,
                        [](std::unique_ptr<ClickerBase> current) {
                            return current->click(
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
                            );
                        }},
                }
            )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("BRANCH ANY COMPLETE"),
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

    void runBranchIfAnyText() {
        std::make_unique<ImageClicker>(
            MarkerA,
            ImageInitConfig{
                .threshold = 0.98f,
                .timeout = 12,
                .region = StageRegion,
            }
        )
            ->branch(
                std::make_unique<IfAnyText>(
                    std::vector<QString>{QStringLiteral("BRANCH C"), QStringLiteral("BRANCH B")},
                    TextUntilConfig{
                        .threshold = 0.25f,
                        .boxThreshold = 0.25f,
                        .region = StageRegion,
                    }
                ),
                BranchMap{
                    {QStringLiteral("BRANCH C"),
                        [](std::unique_ptr<ClickerBase> current) {
                            return current->click(
                                TextRunConfig{
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
                            );
                        }},
                    {QStringLiteral("BRANCH B"),
                        [](std::unique_ptr<ClickerBase> current) {
                            return current->click(
                                TextRunConfig{
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
                            );
                        }},
                }
            )
            ->click(
                ImageRunConfig{
                    .finishUntilList =
                        {
                            new Text(
                                QStringLiteral("BRANCH TEXT COMPLETE"),
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

const std::vector<TestCase>& testCases() {
    static const std::vector<TestCase> cases{
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
            "CONFIRM",
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
        {21,
            "optional-untils",
            "可选 Until 一次检查",
            "UNTIL",
            "IfImage / IfText",
            "4 种 If* 条件均不命中",
            "不阻塞并继续点击",
            "验证 IfImage、IfAnyImage、IfText 和 IfAnyText 只检查一次。"},
        {22,
            "text-match-modes",
            "文字匹配模式链",
            "TEXT",
            "REGEX → FUZZY → EXACT",
            "ORDER-2048 → CONFIRM",
            "MATCH MODES PASS",
            "在同一条文字链中验证正则、模糊和精确匹配。"},
        {23,
            "ordered-selector",
            "有序随机选择器",
            "IMAGE",
            "orderedRandomSelector",
            "4 个同图目标",
            "只选择最右 2 个",
            "先按 X 中心降序排序，再在前两个结果中随机选择。"},
        {24,
            "click-anchor-offset",
            "点击锚点与偏移",
            "ACTION",
            "Click::LEFT + offset",
            "marker-a 左侧锚点",
            "相对坐标 (12, 24)",
            "验证 Segment 锚点和 offset 会合成正确的客户区点击坐标。"},
        {25,
            "reverse-drag",
            "反向纵向拖动",
            "ACTION",
            "drag(reverse=true)",
            "底部 marker-a",
            "向上进入投放区",
            "验证反向拖动、重新匹配与 runUntil 终止。"},
        {26,
            "upward-scroll",
            "滚轮向上单次滚动",
            "ACTION",
            "scroll(delta > 0)",
            "初始位于底部",
            "SCROLL UP COMPLETE",
            "验证正 delta、无 runUntil 时只滚动一次以及 finishUntil 等待。"},
        {27,
            "multiple-until-and",
            "多条件 AND 终止",
            "UNTIL",
            "runUntilList AND",
            "Image + Text",
            "第三次点击后同时满足",
            "所有 runUntil 必须在同一轮满足才能停止循环。"},
        {28,
            "wait-phases",
            "执行阶段等待与归位",
            "LIFECYCLE",
            "startWait / finishWait / homing",
            "两阶段延迟点击",
            "WAIT PHASES PASS",
            "验证动作前等待、后置条件前等待与鼠标归位流程。"},
        {29,
            "branch-if-none",
            "If 未命中继续主链",
            "BRANCH",
            "branch + IfImage",
            "marker-c absent",
            "main chain continues",
            "IfImage 返回 None 时不进入处理函数，继续 branch 后的链。"},
        {30,
            "branch-any-image",
            "AnyImage 目标分支",
            "BRANCH",
            "branch + AnyImage",
            "marker-c | marker-b",
            "marker-c branch",
            "按 AnyImage 实际命中的 target 选择对应子流程并回到图片主链。"},
        {31,
            "branch-if-any-text",
            "IfAnyText 跨类型分支",
            "BRANCH",
            "branch + IfAnyText",
            "BRANCH C | BRANCH B",
            "BRANCH C → Image",
            "文字分支执行后收敛到 ImageClicker，再继续相同主链。"},
    };
    return cases;
}

namespace {

    const TestCase& requireTestCase(
        const int id
    ) {
        for (const TestCase& definition : testCases()) {
            if (definition.id == id) return definition;
        }
        throw std::out_of_range("未知浏览器测试用例: " + std::to_string(id));
    }

    QString jsonWorkflowPath(
        const QString& workflowRoot,
        const TestCase& definition
    ) {
        const QString fileName =
            QStringLiteral("%1-%2.json").arg(definition.id, 2, 10, QLatin1Char('0')).arg(definition.slug);
        return QDir(workflowRoot).filePath(fileName);
    }

    void runCppTestCase(
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
            case 21:
                return runOptionalUntils();
            case 22:
                return runTextMatchModes();
            case 23:
                return runOrderedSelector();
            case 24:
                return runClickAnchorOffset();
            case 25:
                return runReverseDrag();
            case 26:
                return runUpwardScroll();
            case 27:
                return runMultipleUntilAnd();
            case 28:
                return runWaitPhases();
            case 29:
                return runBranchIfNone();
            case 30:
                return runBranchAnyImage();
            case 31:
                return runBranchIfAnyText();
            default:
                throw std::out_of_range("未知浏览器测试用例: " + std::to_string(id));
        }
    }

} // namespace

void validateJsonTestCases(
    const QString& workflowRoot
) {
    for (const TestCase& definition : testCases()) {
        const Workflow workflow = parseWorkflowFile(jsonWorkflowPath(workflowRoot, definition));
        if (workflow.stepCount() == 0) {
            throw std::runtime_error("测试 JSON 工作流不能为空: " + definition.slug.toStdString());
        }
    }
}

void runTestCase(
    const int id,
    const TestWorkflowSource source,
    const QString& workflowRoot
) {
    const TestCase& definition = requireTestCase(id);
    if (source == TestWorkflowSource::CPP) return runCppTestCase(id);

    std::unique_ptr<ClickerBase> last = parseWorkflowFile(jsonWorkflowPath(workflowRoot, definition)).run();
    if (!last) throw std::runtime_error("JSON 工作流未返回最后一个 Clicker");
}
