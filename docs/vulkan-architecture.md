# Vulkan Tutorial — Architecture & Frame Flow

Ce document décrit l’architecture des objets Vulkan de l’application et le flux d’exécution d’une frame. Les diagrammes sont au format Mermaid.

> Astuce VS Code: installe l’extension “Markdown Preview Mermaid Support” pour prévisualiser les diagrammes.

## Architecture des objets

```mermaid
graph LR
  subgraph Windowing
    W[GLFWwindow]
    Surf[VkSurfaceKHR]
    W --> Surf
  end

  Inst[VkInstance] --> PDev[VkPhysicalDevice]
  PDev --> Dev[VkDevice logical]

  Dev --> GQ[VkQueue graphics]
  Dev --> PQ[VkQueue present]

  %% Swapchain and images
  Surf --> Swap[VkSwapchainKHR]
  Dev --> Swap
  Swap --> Img[VkImage array]
  Img --> Views[VkImageView array]
  Views --> Fbs[VkFramebuffer array]

  %% Render pipeline
  Dev --> RP[VkRenderPass]
  Dev --> PL[VkPipelineLayout]
  Dev --> GP[VkPipeline graphics]
  GP --> RP
  GP -. créé depuis .-> VertSM[VkShaderModule vert]
  GP -. créé depuis .-> FragSM[VkShaderModule frag]

  %% Commands & sync
  Dev --> CP[VkCommandPool]
  CP  --> CB[VkCommandBuffer]
  Dev --> Sem[VkSemaphore imageAvailable/renderFinished]
  Dev --> Fn[VkFence inFlight]

  %% Capabilities relation (dashed info link)
  PDev -. requiert formats/presentModes via surface .- Surf
```

Notes:
- GLFW crée la fenêtre et la surface (`VkSurfaceKHR`).
- L’instance → GPU physique → device logique → queues (graphics/present).
- Surface + device → swapchain; images → image views → framebuffers.
- Render pass + pipeline layout → graphics pipeline (les shader modules sont détruits après création).
- Command pool/buffer pour l’enregistrement des commandes.
- Sémaphores et fence pour la synchronisation.

## Flux d’une frame

```mermaid
sequenceDiagram
  participant App as Application (CPU)
  participant Dev as VkDevice
  participant Swap as VkSwapchainKHR
  participant CB as VkCommandBuffer
  participant GQ as GraphicsQueue
  participant PQ as PresentQueue

  App->>Dev: vkWaitForFences(inFlight)
  App->>Dev: vkResetFences(inFlight)
  App->>Swap: vkAcquireNextImageKHR(imageAvailable)
  App->>CB: vkResetCommandBuffer + record(render pass, draw, end)
  App->>GQ: vkQueueSubmit(CB, wait: imageAvailable, signal: renderFinished, fence: inFlight)
  GQ-->>Dev: exécute pipeline et écrit dans le framebuffer
  App->>PQ: vkQueuePresentKHR(wait: renderFinished)
```

## Liens avec le code
- Création window/surface: `initWindow()`, `createSurface()`
- Instance/device/queues: `createInstance()`, `pickPhysicalDevice()`, `createLogicalDevice()`
- Swapchain: `createSwapChain()`, `createImageView()`
- Render pass & pipeline: `createRenderPass()`, `createGraphicsPipeline()`
- Framebuffers: `createFrameBuffers()`
- Commandes: `createCommandPool()`, `createCommandBuffer()`, `recordCommandBuffer()`
- Sync: `createSyncObjects()`, `drawFrame()`
