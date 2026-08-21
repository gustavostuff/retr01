-- 16x4 master palette preview (64 colors).
-- Col 0: greys. Col 1: red. Col 2: skin (only skin-like column).
-- Cols 3-15: yellow -> magenta (skips orange/brown so it does not double skin).

local COLS = 16
local ROWS = 4
local SKIN_COL = 2

local palette = {}

local SKIN = {
  { 74,  44,  30 },
  { 141, 85,  36 },
  { 198, 134, 66 },
  { 241, 194, 125 },
}

local function hsv_to_rgb(h, s, v)
  if s <= 0 then
    return v, v, v
  end
  h = (h % 1) * 6
  local i = math.floor(h)
  local f = h - i
  local p = v * (1 - s)
  local q = v * (1 - f * s)
  local t = v * (1 - (1 - f) * s)
  if i == 0 then return v, t, p
  elseif i == 1 then return q, v, p
  elseif i == 2 then return p, v, t
  elseif i == 3 then return p, q, v
  elseif i == 4 then return t, p, v
  else return v, p, q
  end
end

-- Hues for columns after skin: yellow (0.14) through pink-red (0.98). No orange/brown.
local function post_skin_hue(i, n)
  local lo, hi = 0.14, 0.98
  if n <= 1 then return lo end
  return lo + (hi - lo) * (i / (n - 1))
end

local function fill_hue_col(col, hue)
  for row = 0, ROWS - 1 do
    local value = row / (ROWS - 1)
    local r, g, b = hsv_to_rgb(hue, 0.8, value)
    if row == 0 then
      r, g, b = hsv_to_rgb(hue, 0.8, 0.2)
    end
    palette[row][col] = { r, g, b }
  end
end

local function build_palette()
  for row = 0, ROWS - 1 do
    palette[row] = {}
    local value = row / (ROWS - 1)
    palette[row][0] = { value, value, value }
  end

  fill_hue_col(1, 0) -- red

  for row = 0, ROWS - 1 do
    local s = SKIN[row + 1]
    palette[row][SKIN_COL] = { s[1] / 255, s[2] / 255, s[3] / 255 }
  end

  local n = COLS - 1 - SKIN_COL -- cols 3..15
  for i = 0, n - 1 do
    fill_hue_col(SKIN_COL + 1 + i, post_skin_hue(i, n))
  end
end

function love.load()
  build_palette()
end

function love.draw()
  local w = love.graphics.getWidth() / COLS
  local h = love.graphics.getHeight() / ROWS
  for row = 0, ROWS - 1 do
    for col = 0, COLS - 1 do
      local c = palette[row][col]
      love.graphics.setColor(c[1], c[2], c[3])
      love.graphics.rectangle("fill", col * w, row * h, w + 1, h + 1)
    end
  end
end
