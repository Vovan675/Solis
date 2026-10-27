# Tonemap from Engine/shaders/film.h and Engine/shaders/build_lut.hlsl
import numpy as np

USE_AP1 = True

# HLSL intrinsics
def mul(matrix, color):
    return color @ matrix.T

def lerp(a, b, t):
    return a + (b - a) * t

def saturate(x):
    return np.clip(x, 0.0, 1.0)

def smoothstep(edge0, edge1, x):
    t = saturate((x - edge0) / (edge1 - edge0))
    return t * t * (3.0 - 2.0 * t)

def step(edge, x):
    return (x >= edge).astype(x.dtype)

# Color spaces
REC709_TO_XYZ = np.array([
    [0.4123907993, 0.3575843394, 0.1804807884],
    [0.2126390059, 0.7151686788, 0.0721923154],
    [0.0193308187, 0.1191947798, 0.9505321522]])

XYZ_TO_REC709 = np.array([
    [3.2409699419, -1.5373831776, -0.4986107603],
    [-0.9692436363, 1.8759675015, 0.0415550574],
    [0.0556300797, -0.2039769589, 1.0569715142]])

AP1_TO_XYZ = np.array([
    [0.6624541811, 0.1340042065, 0.1561876870],
    [0.2722287168, 0.6740817658, 0.0536895174],
    [-0.0055746495, 0.0040607335, 1.0103391003]])

XYZ_TO_AP1 = np.array([
    [1.6410233797, -0.3248032942, -0.2364246952],
    [-0.6636628587, 1.6153315917, 0.0167563477],
    [0.0117218943, -0.0082844420, 0.9883948585]])

D65_TO_D60 = np.array([
    [1.0130349146, 0.0061052578, -0.0149709436],
    [0.0076982301, 0.9981633521, -0.0050320385],
    [-0.0028413174, 0.0046851567, 0.9245061375]])

D60_TO_D65 = np.array([
    [0.9872240087, -0.0061132286, 0.0159532883],
    [-0.0075983718, 1.0018614847, 0.0053300358],
    [0.0030725771, -0.0050959615, 1.0816806031]])

REC709_TO_AP1 = XYZ_TO_AP1 @ (D65_TO_D60 @ REC709_TO_XYZ)
AP1_TO_REC709 = XYZ_TO_REC709 @ (D60_TO_D65 @ AP1_TO_XYZ)

def ap1SaturationMatrix(saturation):
    luminance = AP1_TO_XYZ[1]
    grey = np.array([luminance, luminance, luminance])
    identity = np.eye(3)
    return lerp(grey, identity, saturation)

# Khronos PBR Neutral Tone Mapper
@np.errstate(divide='ignore', invalid='ignore')
def PBRNeutralToneMapping(color):
    startCompression = 0.8 - 0.04
    desaturation = 0.15

    x = np.minimum(color[..., 0:1], np.minimum(color[..., 1:2], color[..., 2:3]))
    offset = np.where(x < 0.08, x - 6.25 * x * x, 0.04)
    color = color - offset

    peak = np.maximum(color[..., 0:1], np.maximum(color[..., 1:2], color[..., 2:3]))
    early_return = color # if (peak < startCompression) return color;

    d = 1. - startCompression
    newPeak = 1. - d * d / (peak + d - startCompression)
    color = color * (newPeak / peak)

    g = 1. - 1. / (desaturation * (peak - newPeak) + 1.)
    return np.where(peak < startCompression, early_return, lerp(color, newPeak * np.ones(3), g))

# ACES Curve Fit Approximation
RRT_SAT_MAT = ap1SaturationMatrix(0.96)
ODT_SAT_MAT = ap1SaturationMatrix(0.93)

def RRTAndODTFit(v):
    a = v * (v + 0.0245786) - 0.000090537
    b = v * (0.983729 * v + 0.4329510) + 0.238081
    return a / b

def ACESFitted(color):
    color = mul(RRT_SAT_MAT, color)

    # Apply RRT and ODT
    color = RRTAndODTFit(color)

    color = mul(ODT_SAT_MAT, color)

    return color

# Uchimura 2017, "HDR theory and practice" (aka GT 2017)
def Uchimura(x, P, a, m, l, c, b):
    l0 = ((P - m) * l) / a
    L0 = m - m / a
    L1 = m + (1.0 - m) / a
    S0 = m + l0
    S1 = m + a * l0
    C2 = (a * P) / (P - S1)
    CP = -C2 / P

    w0 = 1.0 - smoothstep(0.0, m, x)
    w2 = step(m + l0, x)
    w1 = 1.0 - w0 - w2

    T = m * np.power(x / m, c) + b
    S = P - (P - S1) * np.exp(CP * (x - S0))
    L = m + a * (x - m)

    return T * w0 + L * w1 + S * w2

# AgX (Blender AgX with Rec.2020 primaries)
REC709_TO_REC2020 = np.array([
    [0.6274, 0.3293, 0.0433],
    [0.0691, 0.9195, 0.0113],
    [0.0164, 0.0880, 0.8956]])

REC2020_TO_REC709 = np.array([
    [1.6605, -0.5876, -0.0728],
    [-0.1246, 1.1329, -0.0083],
    [-0.0182, -0.1006, 1.1187]])

AGX_INSET = np.array([
    [0.856627153315983, 0.0951212405381588, 0.0482516061458583],
    [0.137318972929847, 0.761241990602591, 0.101439036467562],
    [0.11189821299995, 0.0767994186031903, 0.811302368396859]])

AGX_OUTSET = np.array([
    [1.1271005818144368, -0.11060664309660323, -0.016493938717834573],
    [-0.1413297634984383, 1.157823702216272, -0.016493938717834257],
    [-0.14132976349843826, -0.11060664309660294, 1.2519364065950405]])

# Mean error^2: 3.6705141e-06
def agxDefaultContrastApprox(x):
    x2 = x * x
    x4 = x2 * x2

    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232

# Inputs and outputs are encoded as Linear-sRGB.
def AgX(color):
    AgxMinEv = -12.47393 # log2(pow(2, LOG2_MIN) * MIDDLE_GRAY)
    AgxMaxEv = 4.026069 # log2(pow(2, LOG2_MAX) * MIDDLE_GRAY)

    color = mul(REC709_TO_REC2020, color)

    color = mul(AGX_INSET, color)

    # Log2 encoding
    color = np.maximum(color, 1e-10)
    color = np.log2(color)
    color = (color - AgxMinEv) / (AgxMaxEv - AgxMinEv)

    color = saturate(color)

    # Apply sigmoid
    color = agxDefaultContrastApprox(color)

    color = mul(AGX_OUTSET, color)

    # Linearize
    color = np.power(np.maximum(0.0, color), 2.2)

    color = mul(REC2020_TO_REC709, color)

    return color

def LinearToSRGB(l):
    low = l * 12.92
    high = 1.055 * np.power(np.abs(l), 1.0 / 2.4) - 0.055
    return lerp(low, high, step(0.0031308, l))

def apply_tonemap(value, exposure, tonemapper_mode, uchimura):
    color = value * exposure

    if USE_AP1:
        color = mul(REC709_TO_AP1, color)

    # Apply tone mapping
    if tonemapper_mode == 2:
        color = ACESFitted(color)
    elif tonemapper_mode == 3:
        color = Uchimura(color, uchimura['max_brightness'], uchimura['contrast'], uchimura['linear_section_start'],
            uchimura['linear_section_length'], uchimura['black_tightness'], uchimura['pedestal'])

    if USE_AP1:
        color = mul(AP1_TO_REC709, color)

    # Tonemappers with Rec.709 input
    if tonemapper_mode == 1:
        color = PBRNeutralToneMapping(color)
    elif tonemapper_mode == 4:
        color = AgX(color)

    color = saturate(color)

    # Convert to sRGB
    result = LinearToSRGB(color)

    return result
