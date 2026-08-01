# Tonemap from Engine/shaders/film.hlsl
import numpy as np

def Uncharted2(x):
    A = 0.15
    B = 0.50
    C = 0.10
    D = 0.20
    E = 0.02
    F = 0.30
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F

# ACES Curve Fit Approximation
# sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
ACESInputMat = np.array([
    [0.59719, 0.35458, 0.04823],
    [0.07600, 0.90834, 0.01566],
    [0.02840, 0.13383, 0.83777]])

# ODT_SAT => XYZ => D60_2_D65 => sRGB
ACESOutputMat = np.array([
    [1.60475, -0.53108, -0.07367],
    [-0.10208, 1.10813, -0.00605],
    [-0.00327, -0.07276, 1.07602]])

def RRTAndODTFit(v):
    a = v * (v + 0.0245786) - 0.000090537
    b = v * (0.983729 * v + 0.432951) + 0.238081
    return a / b

def ACESFitted(color):
    color = color @ ACESInputMat.T
    # Apply RRT and ODT
    color = RRTAndODTFit(color)
    color = color @ ACESOutputMat.T
    # Clamp to [0, 1]
    return np.clip(color, 0.0, 1.0)

def LinearToSRGB(l):
    low = l * 12.92
    high = 1.055 * np.power(np.abs(l), 1.0 / 2.4) - 0.055
    return np.where(l < 0.0031308, low, high)

def apply_tonemap(value, exposure, tonemapper_mode):
    value = value * exposure
    if tonemapper_mode == 1:
        value = Uncharted2(value)
    elif tonemapper_mode == 2:
        value = ACESFitted(value)
    return np.clip(LinearToSRGB(value), 0.0, 1.0)