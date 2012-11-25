#include "gameboard.h"

#include <gba_dma.h>
#include <gba_input.h>
#include <gba_sound.h>
#include <gba_sprites.h>
#include <gba_video.h>

#include "rng.h"
#include "key.h"
#include "minigame.h"
#include "text.h"
#include "util.h"

#include "tile-palette.h"
#include "tile-data.h"
#include "game-backdrop.h"
#include "hud-sprites.h"

Runloop gameBoard = {
	.init = gameBoardInit,
	.deinit = gameBoardDeinit,
	.frame = gameBoardFrame
};

GameBoard board;
GameParameters currentParams;

const static Sprite bugSprite = {
	.x = 183,
	.y = 119,
	.base = 186,
	.shape = 1,
	.size = 2,
	.palette = 4
};

static u16 timerPalette[16];

static u16 stupidShinyTransitionStates[160] = {};

static int introRow;
static int introBlock;

static void repeatHandler(KeyContext* context, int key);
static KeyContext keyContext = {
	.next = {},
	.active = 0,
	.startDelay = 12,
	.repeatDelay = 8,
	.repeatHandler = repeatHandler
};

static enum {
	LOADING_INTRO,
	PRE_GAMEPLAY,
	GAMEPLAY,
	GAMEPLAY_PAUSED,
	GAMEPLAY_FADE_FOR_MINIGAME
} state;
static u32 startFrame;

static void switchState(int nextState, u32 framecount) {
	state = nextState;
	startFrame = framecount;
	stopRepeat(&keyContext, 0x3FF);
	gameBoard.frame(framecount);
}

static void drawBoard(void) {
	u16* mapData = SCREEN_BASE_BLOCK(1);

	int x, y;
	for (y = 0; y < GAMEBOARD_ROWS; ++y) {
		for (x = 0; x < board.rows[y].width; ++x) {
			mapData[x + y * 32] = 1 | CHAR_PALETTE(board.rows[y].color[x]);
		}
		for (; x < GAMEBOARD_COLS + GAMEBOARD_DEADZONE; ++x) {
			mapData[x + y * 32] = 0;
		}
	}
}

static void resetPlayfield(void) {
	int x, y;
	for (y = 0; y < GAMEBOARD_ROWS; ++y) {
		for (x = 0; x < GAMEBOARD_COLS + GAMEBOARD_DEADZONE; ++x) {
			((u16*) SCREEN_BASE_BLOCK(2))[x + y * 32 + 97] = 5 | CHAR_PALETTE(4);
		}
		((u16*) SCREEN_BASE_BLOCK(2))[y * 32 + 97 + 16] = 6 | CHAR_PALETTE(4);
	}
}

static void resetBackdrop(void) {
	int x, y;
	for (y = 0; y < 20; ++y) {
		for (x = 0; x < 30; ++x) {
			((u16*) SCREEN_BASE_BLOCK(2))[x + y * 32] = (((x + y) % 3) + 2) | CHAR_PALETTE(4);
		}
	}

	resetPlayfield();

	for (y = 0; y < 16; ++y) {
		for (x = 0; x < 6; ++x) {
			if ((y & 3) == 3 && y != 15) {
				((u16*) SCREEN_BASE_BLOCK(2))[x + y * 32 + 119] = 9 | CHAR_PALETTE(4);
			} else if (y & 1 && y != 15) {
				((u16*) SCREEN_BASE_BLOCK(2))[x + y * 32 + 119] = 8 | CHAR_PALETTE(4);
			} else {
				((u16*) SCREEN_BASE_BLOCK(2))[x + y * 32 + 119] = 7 | CHAR_PALETTE(4);
			}
		}
	}

	appendSprite(&(Sprite) {
		.x = 0,
		.y = 16,
		.base = 248,
		.palette = 4,
		.size = 2,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 144,
		.y = 16,
		.base = 248,
		.palette = 5,
		.size = 2,
		.hflip = 1,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 0,
		.y = 104,
		.base = 280,
		.palette = 4,
		.shape = 2,
		.size = 3,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 144,
		.y = 104,
		.base = 280,
		.palette = 5,
		.shape = 2,
		.size = 3,
		.hflip = 1,
		.priority = 1
	});


	for (x = 0; x < 7; ++x) {
		appendSprite(&(Sprite) {
			.x = 16 * x + 24,
			.y = 16,
			.base = 249,
			.palette = 4,
			.shape = 1,
			.size = 0,
			.priority = 1
		});

		appendSprite(&(Sprite) {
			.x = 16 * x + 24,
			.y = 152,
			.base = 473,
			.palette = 4,
			.shape = 1,
			.size = 0,
			.priority = 1
		});
	}


	appendSprite(&(Sprite) {
		.x = 136,
		.y = 16,
		.base = 249,
		.palette = 5,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 136,
		.y = 152,
		.base = 473,
		.palette = 5,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 176,
		.y = 16,
		.base = 248,
		.palette = 4,
		.size = 2,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 208,
		.y = 16,
		.base = 248,
		.palette = 4,
		.size = 2,
		.hflip = 1,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 176,
		.y = 104,
		.base = 280,
		.palette = 4,
		.shape = 2,
		.size = 3,
		.priority = 1
	});

	appendSprite(&(Sprite) {
		.x = 208,
		.y = 104,
		.base = 280,
		.palette = 4,
		.shape = 2,
		.size = 3,
		.hflip = 1,
		.priority = 1
	});

	for (y = 0; y < 3; ++y) {
		appendSprite(&(Sprite) {
			.x = 0,
			.y = 16 * y + 48,
			.base = 280,
			.palette = 4,
			.shape = 2,
			.size = 1,
			.priority = 1
		});

		appendSprite(&(Sprite) {
			.x = 168,
			.y = 16 * y + 48,
			.base = 280,
			.palette = 5,
			.shape = 2,
			.size = 1,
			.hflip = 1,
			.priority = 1
		});

		appendSprite(&(Sprite) {
			.x = 176,
			.y = 16 * y + 48,
			.base = 280,
			.palette = 4,
			.shape = 2,
			.size = 1,
			.priority = 1
		});

		appendSprite(&(Sprite) {
			.x = 232,
			.y = 16 * y + 48,
			.base = 280,
			.palette = 4,
			.shape = 2,
			.size = 1,
			.hflip = 1,
			.priority = 1
		});
	}
}

static void layBlock(void);

void updateScore(void) {
	static char buffer[10] = "000000000\0";

	// TODO: Unhard-code these coordinates?
	if (board.score > 99999) {
		formatNumber(buffer, 9, board.score);
		renderText(buffer, &(Textarea) {
			.destination = TILE_BASE_ADR(2),
			.clipX = 185,
			.clipY = 40,
			.clipW = 64,
			.clipH = 16,
			.baseline = 0
		}, &thinFont);
	} else {
		formatNumber(&buffer[4], 5, board.score);
		renderText(&buffer[4], &(Textarea) {
			.destination = TILE_BASE_ADR(2),
			.clipX = 185,
			.clipY = 40,
			.clipW = 64,
			.clipH = 16,
			.baseline = 0
		}, &largeFont);
	}

	formatNumber(&buffer[4], 5, board.lines);
	renderText(&buffer[4], &(Textarea) {
		.destination = TILE_BASE_ADR(2),
		.clipX = 185,
		.clipY = 72,
		.clipW = 64,
		.clipH = 16,
		.baseline = 0
	}, &largeFont);

	formatNumber(&buffer[6], 3, board.bugs);
	renderText(&buffer[6], &(Textarea) {
		.destination = TILE_BASE_ADR(2),
		.clipX = 203,
		.clipY = 120,
		.clipW = 32,
		.clipH = 16,
		.baseline = 0
	}, &largeFont);

	const char* branch;
	if (board.branch) {
		branch = "* LOCAL";
	} else {
		branch = "* MASTER";
	}
	clearBlock(TILE_BASE_ADR(2), 190, 104, 48, 16);
	renderText(branch, &(Textarea) {
		.destination = TILE_BASE_ADR(2),
		.clipX = 187,
		.clipY = 104,
		.clipW = 64,
		.clipH = 16,
		.baseline = 0
	}, &thinFont);
}

static void updateBlockSprite(Block* block) {
	block->spriteL.palette = block->color;
	block->spriteR.palette = block->color;

	switch (block->width) {
	case 1:
		block->spriteL.shape = 0;
		block->spriteR.disable = 1;
		break;
	case 2:
		block->spriteL.shape = 1;
		block->spriteR.disable = 1;
		break;
	case 3:
		block->spriteL.shape = 1;
		block->spriteR.disable = block->spriteL.disable;
		block->spriteR.shape = 0;
		break;
	case 4:
		block->spriteL.shape = 1;
		block->spriteR.disable = block->spriteL.disable;
		block->spriteR.shape = 1;
		break;
	}
}

static void genBlock(void) {
	u32 seed = rand() >> 16;
	board.active = board.next;
	board.active.spriteL.mode = 1;
	board.active.spriteL.x = 0x88;
	board.active.spriteR.mode = 1;
	board.active.spriteR.x = 0x98;
	board.next.width = (seed & 3) + 1;
	board.next.color = (seed >> 2) & 3;;

	updateBlockSprite(&board.next);
}

static void layRandom(row) {
	u32 seed = rand() >> 16;
	int width = (seed % 3) + 1;
	int color = (seed >> 2) & 3;
	int x;
	for (x = 0; x < width; ++x) {
		board.rows[row].color[x + board.rows[row].width] = color;
	}
	board.rows[row].width += width;
}

static void genRow(int row) {
	int i;
	int y = board.activeY;
	board.activeY = row;
	board.rows[row].width = 0;
	for (i = 0; i < 4; ++i) {
		layRandom(row);
	}
	board.activeY = y;
}

static void resetBoard(void) {
	board.difficultyRamp = 0;
	board.timer = 0;
	board.activeY = 0;
	board.score = 0;
	board.lines = 0;
	board.bugs = 0;

	genBlock();
	genBlock(); // Ensure that a block is actually queued
	int y;
	for (y = 0; y < GAMEBOARD_ROWS; ++y) {
		board.rows[y].width = 0;
	}
}

static void removeRow(u32 framecount) {
	int x;
	Row* row = &board.rows[board.activeY];
	int color = row->color[row->width - 1];
	int score = -board.active.width;
	if (board.difficultyRamp < 256) {
		board.difficultyRamp += currentParams.rampSpeed;
	}
	if (row->width > GAMEBOARD_COLS) {
		board.bugs += row->width - GAMEBOARD_COLS;
		if (board.bugs >= currentParams.bugShuntThreshold) {
			BG_COLORS[0] = 0;
			switchState(GAMEPLAY_FADE_FOR_MINIGAME, framecount);
		}
	}
	for (x = row->width - 1; x >= 0; --x) {
		if (row->color[x] != color) {
			break;
		}
		--row->width;
		++score;
	}
	if (x < 0) {
		++board.lines;
		score *= 2;
		int y;
		for (y = board.activeY; y < GAMEBOARD_ROWS - 1; ++y) {
			board.rows[y] = board.rows[y + 1];
		}
		genRow(GAMEBOARD_ROWS - 1);
	}
	board.score += score;
	REG_SOUND4CNT_L = 0xF200;
	REG_SOUND4CNT_H = 0x8062;
}

static void layBlock(void) {
	int nextWidth = board.rows[board.activeY].width + board.active.width;
	int i;
	if (currentParams.hasColorMismatchBugs && board.rows[board.activeY].color[board.rows[board.activeY].width - 1] != board.active.color) {
		++board.bugs;
		updateScore();
	}
	for (i = board.rows[board.activeY].width; i < nextWidth; ++i) {
		board.rows[board.activeY].color[i] = board.active.color;
	}
	board.rows[board.activeY].width = i;
}

static void dropBlock(u32 framecount) {
	REG_SOUND1CNT_L = 0x001F;
	REG_SOUND1CNT_H = 0xE2B4;
	REG_SOUND1CNT_X = 0x8500;
	layBlock();
	if (board.rows[board.activeY].width >= GAMEBOARD_COLS) {
		removeRow(framecount);
		updateScore();
	}
	if (board.bugs >= currentParams.bugShuntThreshold) {
		BG_COLORS[0] = 0;
		switchState(GAMEPLAY_FADE_FOR_MINIGAME, framecount);
	}
	genBlock();
	drawBoard();

	board.timer = 0;
}

static void updateBlocks(void) {
	board.active.spriteL.y = board.active.spriteR.y = 160 - 8 - 8 * GAMEBOARD_ROWS + (board.activeY << 3);
	board.next.spriteL.x = 0xC4 + (3 - board.next.width) * 4;
	board.next.spriteR.x = 0xD4 + (3 - board.next.width) * 4;
	board.active.spriteL.disable = 0;
	board.next.spriteL.disable = 0;
	updateBlockSprite(&board.active);
	updateBlockSprite(&board.next);
}

static void updateTimer(u32 framecount) {
	++board.timer;
	int i;
	int timerMax = ramp(currentParams.dropTimerLength, currentParams.dropTimerMin);
	for (i = 0; i < 16; ++i) {
		OBJ_COLORS[16 * 5 + i] = timerPalette[i] + (board.timer << 4) / timerMax;
	}
	if (board.timer >= timerMax) {
		dropBlock(framecount);
	}
}

static void blockUp(void) {
	--board.activeY;
	if (board.activeY < 0) {
		board.activeY = GAMEBOARD_ROWS - 1;
	}
	REG_SOUND1CNT_L = 0x0027;
	REG_SOUND1CNT_H = 0xA1B4;
	REG_SOUND1CNT_X = 0x8500;
}

static void blockDown(void) {
	++board.activeY;
	if (board.activeY >= GAMEBOARD_ROWS) {
		board.activeY = 0;
	}
	REG_SOUND1CNT_L = 0x0027;
	REG_SOUND1CNT_H = 0xA1B4;
	REG_SOUND1CNT_X = 0x8440;
}

static void hideBoard(void) {
	board.active.spriteL.disable = 1;
	board.active.spriteR.disable = 1;
	board.next.spriteL.disable = 1;
	board.next.spriteR.disable = 1;
	updateSprite(&board.active.spriteL, 0);
	updateSprite(&board.active.spriteR, 1);
	updateSprite(&board.next.spriteL, 2);
	updateSprite(&board.next.spriteR, 3);
	writeSpriteTable();
}

static void repeatHandler(KeyContext* context, int keys) {
	(void) (context);
	if (keys & KEY_UP) {
		blockUp();
	}
	if (keys & KEY_DOWN) {
		blockDown();
	}
}

void gameBoardInit(u32 framecount) {
	// TODO: store this in RAM so we don't have to copy it out of the cart each time
	DMA3COPY(tileTiles, TILE_BASE_ADR(0) + 32, DMA16 | DMA_IMMEDIATE | (tileTilesLen >> 1));
	DMA3COPY(tileTiles, OBJ_BASE_ADR, DMA16 | DMA_IMMEDIATE | (tileTilesLen >> 1));
	DMA3COPY(tileTiles, OBJ_BASE_ADR + 32, DMA16 | DMA_IMMEDIATE | (tileTilesLen >> 1));

	DMA3COPY(game_backdropTiles, TILE_BASE_ADR(0) + 64, DMA16 | DMA_IMMEDIATE | (game_backdropTilesLen >> 1));

	DMA3COPY(hud_spritesTiles, TILE_BASE_ADR(4) + 0x1400, DMA16 | DMA_IMMEDIATE | (hud_spritesTilesLen >> 1));

	clearSpriteTable();
	board.next.spriteL.raw.a = 0x428C;
	board.next.spriteL.priority = 1;
	board.next.spriteR.raw.a = 0x428C;
	board.next.spriteR.priority = 1;
	board.active.spriteL.disable = 1;
	board.active.spriteR.disable = 1;
	insertSprite(&board.next.spriteL, 0);
	insertSprite(&board.next.spriteR, 1);
	insertSprite(&board.next.spriteL, 2);
	insertSprite(&board.next.spriteR, 3);
	appendSprite(&bugSprite);
	writeSpriteTable();

	resetBackdrop();

	mapText(SCREEN_BASE_BLOCK(3), 0, 32, 0, 20, 5);

	// TODO: Move to constants
	renderText("SCORE", &(Textarea) {
		.destination = TILE_BASE_ADR(2),
		.clipX = 186,
		.clipY = 24,
		.clipW = 64,
		.clipH = 16,
		.baseline = 0
	}, &largeFont);

	renderText("LINES", &(Textarea) {
		.destination = TILE_BASE_ADR(2),
		.clipX = 192,
		.clipY = 56,
		.clipW = 64,
		.clipH = 16,
		.baseline = 0
	}, &largeFont);

	renderText("ON BRANCH", &(Textarea) {
		.destination = TILE_BASE_ADR(2),
		.clipX = 186,
		.clipY = 88,
		.clipW = 64,
		.clipH = 16,
		.baseline = 0
	}, &thinFont);

	renderText("CLONING REPOSITORY", &(Textarea) {
		.destination = TILE_BASE_ADR(2),
		.clipX = 12,
		.clipY = 76,
		.clipW = 160,
		.clipH = 16,
		.baseline = 0
	}, &largeFont);

	introRow = 0;
	introBlock = 0;

	srand(framecount);
	resetBoard();
	updateScore();
	switchState(LOADING_INTRO, framecount);
	minigameInit(framecount);
	gameBoardSetup(framecount);

	drawBoard();

	DMA3COPY(game_backdropPal, &BG_COLORS[16 * 4], DMA16 | DMA_IMMEDIATE | (game_backdropPalLen >> 1));
	DMA3COPY(hud_spritesPal, &BG_COLORS[16 * 5], DMA16 | DMA_IMMEDIATE | (hud_spritesPalLen >> 2));
	DMA3COPY(hud_spritesPal, &OBJ_COLORS[16 * 4], DMA16 | DMA_IMMEDIATE | (hud_spritesPalLen >> 1));
	DMA3COPY(hud_spritesPal, &OBJ_COLORS[16 * 5], DMA16 | DMA_IMMEDIATE | (hud_spritesPalLen >> 1));
	DMA3COPY(hud_spritesPal, timerPalette, DMA16 | DMA_IMMEDIATE | (hud_spritesPalLen >> 1));
}

void gameBoardDeinit() {
	clearSpriteTable();
}

void gameBoardFrame(u32 framecount) {
	scanKeys();
	u16 keys = keysDown();
	u16 unkeys = keysUp();
	static char buffer[5] = "000%\0";

	switch (state) {
	case LOADING_INTRO:
		formatNumber(buffer, 3, 100 * (framecount - startFrame) / (GAMEBOARD_ROWS * 8 + 1));
		renderText(buffer, &(Textarea) {
			.destination = TILE_BASE_ADR(2),
			.clipX = 188,
			.clipY = 136,
			.clipW = 64,
			.clipH = 16,
			.baseline = 0
		}, &largeFont);
		if (!((framecount - startFrame + 1) & 1)) {
			if (introRow == GAMEBOARD_ROWS) {
				clearBlock(TILE_BASE_ADR(2), 188, 136, 64, 16);
				clearBlock(TILE_BASE_ADR(2), 12, 72, 160, 16);
				switchState(PRE_GAMEPLAY, framecount);
			} else {
				layRandom(introRow);
				++introBlock;
				if (introBlock == 4) {
					introBlock = 0;
					++introRow;
				}
				drawBoard();
			}
		}
		break;
	case PRE_GAMEPLAY:
		if (framecount == startFrame + 1) {
			updateBlocks();
			unmapText(SCREEN_BASE_BLOCK(3), 1, 22, 9, 12);
			renderText("BEGIN PROGRAMMING!", &(Textarea) {
				.destination = TILE_BASE_ADR(2),
				.clipX = 11,
				.clipY = 76,
				.clipW = 160,
				.clipH = 16,
				.baseline = 0
			}, &largeFont);
			mapText(SCREEN_BASE_BLOCK(3), 1, 22, 9, 12, 5);
		} else if (framecount - startFrame == 120 || keys) {
			unmapText(SCREEN_BASE_BLOCK(3), 1, 22, 9, 12);
			clearBlock(TILE_BASE_ADR(2), 11, 76, 160, 16);
			switchState(GAMEPLAY, framecount);
		}
		break;
	case GAMEPLAY_PAUSED:
		if (keys & KEY_START) {
			board.active.spriteL.mode = 1;
			board.active.spriteR.mode = 1;
			REG_BLDCNT = 0x0100;
			switchState(GAMEPLAY, framecount);
		}
		break;
	case GAMEPLAY:
		if (unkeys) {
			stopRepeat(&keyContext, unkeys);
		}

		if (keys & KEY_START) {
			board.active.spriteL.mode = 0;
			board.active.spriteR.mode = 0;
			REG_BLDCNT = 0x01FF;
			REG_BLDY = 0x000A;
			switchState(GAMEPLAY_PAUSED, framecount);
		}

		if (keys & KEY_UP) {
			startRepeat(&keyContext, framecount, KEY_UP);
			blockUp();
		}

		if (keys & KEY_DOWN) {
			startRepeat(&keyContext, framecount, KEY_DOWN);
			blockDown();
		}

		doRepeat(&keyContext, framecount);

		if (keys & KEY_A && board.timer > 9) {
			dropBlock(framecount);
		}

		if (keys & KEY_L) {
			dropBlock(framecount);
			board.difficultyRamp -= 16;
			if (board.difficultyRamp < 0) {
				board.difficultyRamp = 0;
			}
		}

		if (keys & KEY_R) {
			dropBlock(framecount);
			board.difficultyRamp += 16;
			if (board.difficultyRamp > 256) {
				board.difficultyRamp = 256;
			}
		}

		if (keys & KEY_SELECT) {
			board.branch = !board.branch;
			updateScore();
		}

		updateTimer(framecount);
		updateBlocks();

		if (keys & KEY_B && board.bugs >= currentParams.bugEntryThreshold) {
			BG_COLORS[0] = 0;
			switchState(GAMEPLAY_FADE_FOR_MINIGAME, framecount);
		}
		break;
	case GAMEPLAY_FADE_FOR_MINIGAME:
		REG_DMA0CNT = 0;
		if (framecount - startFrame < 48) {
			int i;
			for (i = 16; i < 152; ++i) {
				if (i & 1) {
					int state = 6 * (framecount - startFrame) + (i >> 1);
					state -= 72;
					if (state < 8) {
						state = 8;
					}
					if (state > 168) {
						state = 168;
					}
					stupidShinyTransitionStates[i] = 0x0800 + state;
				} else {
					int state = 6 * (framecount - startFrame) - (i >> 1);
					state = 168 - state;
					if (state < 8) {
						state = 8;
					}
					if (state > 168) {
						state = 168;
					}
					stupidShinyTransitionStates[i] = 0x00A8 + (state << 8);
				}
			}
			REG_WININ = 0x3B00;
			REG_WINOUT = 0x003F;
			DMA0COPY(stupidShinyTransitionStates, &REG_WIN0H, DMA16 | DMA_REPEAT | DMA_HBLANK | DMA_SRC_INC | DMA_DST_FIXED | 1);
		} else {
			REG_WININ = 0x3B3F;
			REG_WINOUT = 0x001B;
			REG_WIN0H = 0x08A8;
			gameBoard.frame = minigameFrame;
			hideBoard();
			showMinigame(framecount);
			switchState(PRE_GAMEPLAY, framecount);
		}
		break;
	}

	REG_BLDALPHA = 0x0F0B;
	updateSprite(&board.active.spriteL, 0);
	updateSprite(&board.active.spriteR, 1);
	updateSprite(&board.next.spriteL, 2);
	updateSprite(&board.next.spriteR, 3);
	writeSpriteTable();
}

void gameBoardSetup(u32 framecount) {
	DMA3COPY(tile_bluePal + 1, &BG_COLORS[1], DMA16 | DMA_IMMEDIATE | (16 * 4 - 1));
	DMA3COPY(tile_bluePal, &OBJ_COLORS[0], DMA16 | DMA_IMMEDIATE | (16 * 4));
	resetPlayfield();
	drawBoard();

	startFrame = framecount;

	REG_BG0CNT = CHAR_BASE(0) | SCREEN_BASE(2) | 2;
	REG_BG1CNT = CHAR_BASE(2) | SCREEN_BASE(3);
	REG_BG2CNT = CHAR_BASE(0) | SCREEN_BASE(1) | 1;
	REG_BG2HOFS = -8;
	REG_BG2VOFS = -24;
	REG_DISPCNT = MODE_0 | BG0_ON | BG1_ON | BG2_ON | OBJ_ON | WIN0_ON;
	REG_BLDCNT = 0x0100;

	REG_WIN0H = 0x08A8;
	REG_WIN0V = 0x1898;
	REG_WININ = 0x3B3F;
	REG_WINOUT = 0x001B;

	REG_SOUNDCNT_X = 0x80;
	REG_SOUNDCNT_L = SND1_R_ENABLE | SND1_L_ENABLE | SND4_R_ENABLE | SND4_L_ENABLE | 0x33;
}

inline int ramp(int a, int b) {
	return (a * (256 - board.difficultyRamp) + b * board.difficultyRamp) >> 8;
}