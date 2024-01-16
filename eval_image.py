import cv2
import numpy as np
from skimage.metrics import structural_similarity
from skimage.metrics import mean_squared_error

# https://www.mathworks.com/help/vision/ref/psnr.html
def calculate_psnr(image1, image2):
    mse = mean_squared_error(image1, image2)
    if mse == 0:
        return float('inf')
    r = 255.0
    psnr = 10 * np.log10(pow(r, 2) / mse)
    return psnr

def calculate_ssim(image1, image2):
    return structural_similarity(image1, image2, multichannel=True)
def calculate_mean_square_error(image1, image2):
    return mean_squared_error(image1, image2)

def calculate_root_mean_square_error(image1, image2):
    mse = mean_squared_error(image1, image2)
    rmse = np.sqrt(mse)
    return rmse

def main():
    # Load the images
    image1 = cv2.imread('rendered_image_target.png')
    image2 = cv2.imread('rendered_image_target.png')

    # Convert images to grayscale for SSIM
    gray1 = cv2.cvtColor(image1, cv2.COLOR_BGR2GRAY)
    gray2 = cv2.cvtColor(image2, cv2.COLOR_BGR2GRAY)

    # Calculate metrics
    psnr_value = calculate_psnr(image1, image2)
    ssim_value = calculate_ssim(gray1, gray2)
    rmse_value = calculate_root_mean_square_error(gray1, gray2)

    print(f"PSNR: {psnr_value}")
    print(f"SSIM: {ssim_value}")
    print(f"rmse_value: {rmse_value}")

if __name__ == "__main__":
    main()
