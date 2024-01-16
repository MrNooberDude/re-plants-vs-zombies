#include "VkImage.h"
#include "Common.h"
#include "TriVertex.h"
#include "VkCommon.h"
#include "misc/SexyMatrix.h"
#include <array>
#include <chrono>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

#include <glm/gtx/string_cast.hpp>

namespace Vk {

int descriptorPoolSize = 0;
VkImage::VkImage(ImageLib::Image &theImage) {
    renderMutex.lock();

    mWidth = theImage.mWidth;
    mHeight = theImage.mHeight;

    if (!mWidth || !mHeight) throw std::runtime_error("Images with no size are not supported.");

    VkDeviceSize imageSize = mWidth * mHeight * sizeof(uint32_t);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(
        imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory
    );

    void *data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, theImage.mBits.get(), imageSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createImage(
        mWidth, mHeight, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory
    );

    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        endSingleTimeCommands(commandBuffer);
    }

    copyBufferToImage(stagingBuffer, image, mWidth, mHeight);

    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        endSingleTimeCommands(commandBuffer);
    }

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    view = createImageView(image, VK_FORMAT_B8G8R8A8_SRGB);

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = imagePass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &view;
    framebufferInfo.width = mWidth;
    framebufferInfo.height = mHeight;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    vkAllocateDescriptorSets(device, &allocInfo, &descriptor);
    ++descriptorPoolSize;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = view;
    imageInfo.sampler = textureSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptor;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    renderMutex.unlock();
}

VkImage::~VkImage() {
    renderMutex.lock();
    flushCommandBuffer();
    vkDeviceWaitIdle(device);
    vkDestroyImageView(device, view, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, memory, nullptr);
    renderMutex.unlock();
}

std::map<VkImageLayout, std::pair<VkAccessFlags, VkPipelineStageFlags>> accessMaskMap = {
    {VK_IMAGE_LAYOUT_UNDEFINED,                {0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT}                            },
    {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,     {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT}    },
    {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, {VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
    {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
     {VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}                                                             },
};

void TransitionImageLayouts(VkCommandBuffer commandBuffer, std::vector<std::pair<VkImage *, VkImageLayout>> images) {
    std::vector<VkImageMemoryBarrier> barriers;

    VkPipelineStageFlags srcStageMask{};
    VkPipelineStageFlags dstStageMask{};

    for (auto image : images) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = image.first->layout;
        barrier.newLayout = image.second;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barrier.image = image.first->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        auto srcAccess = accessMaskMap[image.first->layout];
        auto dstAccess = accessMaskMap[image.second];

        barrier.srcAccessMask = srcAccess.first;
        barrier.dstAccessMask = dstAccess.first;
        srcStageMask |= srcAccess.second;
        dstStageMask |= dstAccess.second;

        image.first->layout = image.second;

        barriers.push_back(barrier);
    }

    vkCmdPipelineBarrier(
        commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data()
    );
}

void VkImage::TransitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout) {
    TransitionImageLayouts(
        commandBuffer,
        {
            {this, newLayout}
    }
    );
}

/*================*
 | DRAW FUNCTIONS |
 *================*/

int imageBufferIdx = 0;
bool inRenderpass = false;
int num_passes = 0;
void endRenderPass() {
    if (inRenderpass) {
        num_passes++;
        vkCmdEndRenderPass(imageCommandBuffers[imageBufferIdx]);
        inRenderpass = false;
    }
}

bool inRecording = false;
auto begin_time = std::chrono::high_resolution_clock::now();
void flushCommandBuffer() {
    if (inRecording) {
        endRenderPass();

        vkEndCommandBuffer(imageCommandBuffers[imageBufferIdx]);
        inRecording = false;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &imageCommandBuffers[imageBufferIdx];

        vkWaitForFences(device, 1, &imageFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &imageFence);
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, imageFence);
        imageBufferIdx = (imageBufferIdx + 1) % num_image_swaps;

        vkResetCommandBuffer(imageCommandBuffers[imageBufferIdx], 0);
    }
}

void VkImage::FillRect(const Rect & /*theRect*/, const Color & /*theColor*/, int /*theDrawMode*/) {
    // Blt(Sexy::IMAGE_BLANK, theRect.mX, theRect.mY, Rect{0, 0, theRect.mWidth, theRect.mHeight}, theColor,
    // theDrawMode);

    /*
        renderMutex.lock();

        auto color = VkClearColorValue{ .float32 = {theColor.mRed/255.0f, theColor.mGreen/255.0f, theColor.mBlue/255.0f}
       };

        auto subresourceRange = VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(imageCommandBuffers[imageBufferIdx], image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color,
       1, &subresourceRange);

        renderMutex.unlock();
    */
}

glm::vec4 RectToVec4(Rect a) { return glm::vec4(a.mX, a.mY, a.mWidth, a.mHeight); }

VkImage *otherCachedImage = nullptr;
VkImage *thisCachedImage = nullptr;
void VkImage::Blt(Image *theImage, int theX, int theY, const Rect &theSrcRect, const Color &theColor, int theDrawMode) {
    glm::mat4 theMatrix =
        glm::translate(glm::mat4(1.0), glm::vec3(theX + theSrcRect.mWidth / 2.0, theY + theSrcRect.mHeight / 2.0, 0.0));
    glm::vec4 theClipRect = {0, 0, mWidth, mHeight};
    BltEx(theImage, theMatrix, RectToVec4(theSrcRect), theClipRect, theColor, theDrawMode, true);
}

void VkImage::BltF(
    Image *theImage, float theX, float theY, const Rect &theSrcRect, const Rect &theClipRect, const Color &theColor,
    int theDrawMode
) {
    // glm::vec4 theDestRect = {theX, theY, theSrcRect.mWidth, theSrcRect.mHeight};

    glm::mat4 theMatrix =
        glm::translate(glm::mat4(1.0), glm::vec3(theX + theSrcRect.mWidth / 2.0, theY + theSrcRect.mHeight / 2.0, 0.0));

    BltEx(theImage, theMatrix, RectToVec4(theSrcRect), RectToVec4(theClipRect), theColor, theDrawMode, true);
}

void VkImage::BltMatrix(
    Image *theImage, float /*x*/, float /*y*/, const SexyMatrix3 &theMatrix, const Rect &theClipRect,
    const Color &theColor, int theDrawMode, const Rect &theSrcRect, bool blend
) {
    glm::mat4 matrix = glm::mat4(
        theMatrix.m00, theMatrix.m10, theMatrix.m20, 0, theMatrix.m01, theMatrix.m11, theMatrix.m21, 0, theMatrix.m02,
        theMatrix.m12, theMatrix.m22, 0, 0, 0, 0, 1
    );

    BltEx(theImage, matrix, RectToVec4(theSrcRect), RectToVec4(theClipRect), theColor, theDrawMode, blend);
}

void VkImage::BeginDraw(Image *theImage) {
    VkImage *otherImage = dynamic_cast<VkImage *>(theImage);
    bool thisCacheMiss = (this != thisCachedImage);
    bool otherCacheMiss = (otherImage != otherCachedImage);
    thisCachedImage = this;
    otherCachedImage = otherImage;

    bool thisLayoutSuboptimal = (layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    bool otherLayoutSuboptimal = (otherImage->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (!inRecording) { // Start recording the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(imageCommandBuffers[imageBufferIdx], &beginInfo);
        vkCmdBindPipeline(imageCommandBuffers[imageBufferIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        inRecording = true;
    }

    if (thisCacheMiss || otherCacheMiss) endRenderPass();

    if (thisLayoutSuboptimal || otherLayoutSuboptimal) {
        std::vector<std::pair<VkImage *, VkImageLayout>> transitions;

        if (thisLayoutSuboptimal) transitions.push_back({this, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        if (otherLayoutSuboptimal) transitions.push_back({otherImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

        TransitionImageLayouts(imageCommandBuffers[imageBufferIdx], transitions);
    }

    if (!inRenderpass) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = imagePass;
        renderPassInfo.framebuffer = framebuffer;

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {
            static_cast<uint32_t>(mWidth),
            static_cast<uint32_t>(mHeight),
        };
        vkCmdBeginRenderPass(imageCommandBuffers[imageBufferIdx], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        inRenderpass = true;

        vkCmdBindDescriptorSets(
            imageCommandBuffers[imageBufferIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
            &otherImage->descriptor, 0, nullptr
        );
    }
}

void VkImage::SetViewportAndScissor(const glm::vec4 &theClipRect) {
    VkViewport viewport = {0, 0, static_cast<float>(mWidth), static_cast<float>(mHeight), 0.0, 1.0};
    vkCmdSetViewport(imageCommandBuffers[imageBufferIdx], 0, 1, &viewport);

    VkRect2D scissor = {
        {static_cast<int32_t>(theClipRect.x), static_cast<int32_t>(theClipRect.y)},
        {(uint32_t)(theClipRect.z),           (uint32_t)(theClipRect.w)          }
    };
    vkCmdSetScissor(imageCommandBuffers[imageBufferIdx], 0, 1, &scissor);
}

void VkImage::BltEx(
    Image *theImage, const glm::mat4 &theMatrix, const glm::vec4 &theSrcRect, const glm::vec4 &theClipRect,
    const Color &theColor, int, bool
) {
    if (theClipRect.z <= 0 || theClipRect.w <= 0) return; // Can't draw regions with negative size.
    renderMutex.lock();

    VkImage::BeginDraw(theImage);

    float w2 = theSrcRect.z / 2.0;
    float h2 = theSrcRect.w / 2.0;

    float u0 = theSrcRect.x / theImage->mWidth;
    float v0 = theSrcRect.y / theImage->mHeight;
    float u1 = (theSrcRect.x + theSrcRect.z) / theImage->mWidth;
    float v1 = (theSrcRect.y + theSrcRect.w) / theImage->mHeight;

    glm::vec4 vertices[4] = {
        {-w2, -h2, u0, v0},
        {w2,  -h2, u1, v0},
        {-w2, h2,  u0, v1},
        {w2,  h2,  u1, v1},
    };

    for (auto &vert : vertices) {
        glm::vec4 v(vert.x, vert.y, 1, 1);
        v = theMatrix * v;
        vert.x = 2 * (v.x / mWidth) - 1;
        vert.y = 2 * (v.y / mHeight) - 1;
    }

    SetViewportAndScissor(theClipRect);

    ImagePushConstants constants = {
        {vertices[0], vertices[1], vertices[2], vertices[3]},
        {0,           0,           0,           0          },
        glm::vec4(theColor.mRed / 255.0, theColor.mGreen / 255.0, theColor.mBlue / 255.0, theColor.mAlpha / 255.0),
        true,
    };

    vkCmdPushConstants(
        imageCommandBuffers[imageBufferIdx], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(ImagePushConstants), &constants
    );

    vkCmdDraw(imageCommandBuffers[imageBufferIdx], 6, 1, 0, 0);

    renderMutex.unlock();
}

void VkImage::StretchBlt(
    Image *theImage, const Rect &theDestRect, const Rect &theSrcRect, const Rect &theClipRect, const Color &theColor,
    int theDrawMode, bool
) {
    glm::mat4 theMatrix = glm::scale(
        glm::translate(
            glm::mat4(1.0),
            glm::vec3(theDestRect.mX + theSrcRect.mWidth / 2.0, theDestRect.mY + theSrcRect.mHeight / 2.0, 0.0)
        ),
        glm::vec3((float)theDestRect.mWidth / theSrcRect.mWidth, (float)theDestRect.mHeight / theSrcRect.mHeight, 1.0)
    );

    BltEx(theImage, theMatrix, RectToVec4(theSrcRect), RectToVec4(theClipRect), theColor, theDrawMode, true);
}

void VkImage::BltTrianglesTex(
    Image *theTexture, const TriVertex theVertices[][3], int theNumTriangles, const Rect &theClipRect,
    const Color &theColor, int /*theDrawMode*/, float tx, float ty, bool /*blend*/
) {
    if (theClipRect.mWidth <= 0 || theClipRect.mHeight <= 0) return; // Can't draw regions with negative size.
    renderMutex.lock();

    VkImage::BeginDraw(theTexture);

    std::vector<glm::vec4> vertices;
    std::vector<uint32_t> colors;
    vertices.reserve(theNumTriangles * 3);
    for (int i = 0; i < theNumTriangles; ++i) {
        auto &triangle = theVertices[i];

        for (int j = 0; j < 3; ++j) {
            vertices.push_back(glm::vec4(
                2 * (triangle[j].x + tx) / mWidth - 1, 2 * (triangle[j].y + ty) / mHeight - 1, triangle[j].u,
                triangle[j].v
            ));

            colors.push_back(triangle[j].color);
        }
    }

    SetViewportAndScissor(RectToVec4(theClipRect));

    for (int i = 0; i < theNumTriangles; ++i) {
        ImagePushConstants constants = {
            {vertices[3 * i], vertices[3 * i + 1], vertices[3 * i + 2]},
            {colors[3 * i],   colors[3 * i + 1],   colors[3 * i + 2]  },
            glm::vec4(theColor.mRed / 255.0, theColor.mGreen / 255.0, theColor.mBlue / 255.0, theColor.mAlpha / 255.0),
            false,
        };

        vkCmdPushConstants(
            imageCommandBuffers[imageBufferIdx], pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ImagePushConstants), &constants
        );
        vkCmdDraw(imageCommandBuffers[imageBufferIdx], 3, 1, 0, 0);
    }

    renderMutex.unlock();
}

void VkImage::BltRotated(Image *, float, float, const Rect &, const Rect &, const Color &, int, double, float, float) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::BltRotated is a stub.\n");
    has_shown = true;
}

void VkImage::BltMirror(Image *, int, int, const Rect &, const Color &, int) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::BltMirror is a stub.\n");
    has_shown = true;
}

void VkImage::StretchBltMirror(Image *, const Rect &, const Rect &, const Rect &, const Color &, int, bool) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::StretchBltMirror is a stub.\n");
    has_shown = true;
}

/*=================*
 |  UNIMPLEMENTED  |
 *=================*/

void VkImage::ClearRect(const Rect & /*theRect*/) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::ClearRect is a stub.\n");
    has_shown = true;
}

void VkImage::DrawLine(double, double, double, double, const Color &, int) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::DrawLine is a stub.\n");
    has_shown = true;
}

void VkImage::DrawLineAA(double, double, double, double, const Color &, int) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::DrawLineAA is a stub.\n");
    has_shown = true;
}

void VkImage::FillScanLinesWithCoverage(Span *, int, const Color &, int, const BYTE *, int, int, int, int) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::FillScanLinesWithCoverage is a stub.\n");
    has_shown = true;
}

bool VkImage::PolyFill3D(const Point *, int, const Rect *, const Color &, int, int, int) {
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::PolyFill3D is a stub.\n");
    has_shown = true;

    return false;
}

/*
void VkImage::StretchBlt(Image*, const Rect&, const Rect&, const Rect&, const Color&, int, bool){
    static bool has_shown = false;
    if (!has_shown) printf("draw:     VkImage::StretchBlt is a stub.\n");
    has_shown = true;
}*/

} // namespace Vk

/*
void Image::FillScanLinesWithCoverage(Span* theSpans, int theSpanCount, const Color& theColor, int theDrawMode, const
BYTE* theCoverage, int theCoverX, int theCoverY, int theCoverWidth, int theCoverHeight); bool Image::PolyFill3D(const
Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int
ty, bool convex); void Image::Blt(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor,
int theDrawMode); void Image::BltF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect
&theClipRect, const Color& theColor, int theDrawMode); void Image::BltRotated(Image* theImage, float theX, float theY,
const Rect &theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, double theRot, float
theRotCenterX, float theRotCenterY); void Image::StretchBlt(Image* theImage, const Rect& theDestRect, const Rect&
theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch); void
Image::BltMatrix(Image* theImage, float x, float y, const SexyMatrix3 &theMatrix, const Rect& theClipRect, const Color&
theColor, int theDrawMode, const Rect &theSrcRect, bool blend); void Image::BltTrianglesTex(Image *theTexture, const
TriVertex theVertices[][3], int theNumTriangles, const Rect& theClipRect, const Color &theColor, int theDrawMode, float
tx, float ty, bool blend); void Image::BltMirror(Image* theImage, int theX, int theY, const Rect& theSrcRect, const
Color& theColor, int theDrawMode); void Image::StretchBltMirror(Image* theImage, const Rect& theDestRect, const Rect&
theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch);
*/
