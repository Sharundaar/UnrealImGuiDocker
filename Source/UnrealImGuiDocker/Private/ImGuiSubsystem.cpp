#include "ImGuiSubsystem.h"

#include <string>

#include "Editor.h"
#include "imgui.h"
#include "Selection.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"

UE_DISABLE_OPTIMIZATION

// Copy of ImDrawList for safekeeping 
struct FImGuiDrawList
{
	TArray<ImDrawCmd> CmdBuffer;
	TArray<ImDrawIdx> IdxBuffer;
	TArray<ImDrawVert> VtxBuffer;
	ImDrawListFlags Flags;

	void Set(ImDrawList* DrawList);
};

void FImGuiDrawList::Set(ImDrawList* DrawList)
{
	for(int i=0; i<DrawList->CmdBuffer.size(); ++i)
	{
		CmdBuffer.Add(DrawList->CmdBuffer[i]);
	}

	for(int i=0; i<DrawList->IdxBuffer.size(); ++i)
    {
    	IdxBuffer.Add(DrawList->IdxBuffer[i]);
    }
	
	for(int i=0; i<DrawList->VtxBuffer.size(); ++i)
	{
		VtxBuffer.Add(DrawList->VtxBuffer[i]);
	}

	Flags = DrawList->Flags;
}

struct FImGuiDrawData
{
	int                 CmdListsCount = 0;      // Number of ImDrawList* to render
	int                 TotalIdxCount;      // For convenience, sum of all ImDrawList's IdxBuffer.Size
	int                 TotalVtxCount;      // For convenience, sum of all ImDrawList's VtxBuffer.Size
	TArray<FImGuiDrawList> CmdLists;         // Array of ImDrawList to render.
	ImVec2              DisplayPos;         // Top-left position of the viewport to render (== top-left of the orthogonal projection matrix to use) (== GetMainViewport()->Pos for the main viewport, == (0.0) in most single-viewport applications)
	ImVec2              DisplaySize;        // Size of the viewport to render (== GetMainViewport()->Size for the main viewport, == io.DisplaySize in most single-viewport applications)
	ImVec2              FramebufferScale;   // Amount of pixels for each unit of DisplaySize. Based on io.DisplayFramebufferScale. Generally (1,1) on normal display, (2,2) on OSX with Retina display.
	ImGuiViewport*      OwnerViewport;      // Viewport carrying the ImDrawData instance, might be of use to the renderer (generally not).

	void Set(ImDrawData* DrawData);
};

void FImGuiDrawData::Set(ImDrawData* DrawData)
{
	check(DrawData->Valid);
	
	CmdListsCount = DrawData->CmdListsCount;
	TotalIdxCount = DrawData->TotalIdxCount;
	TotalVtxCount = DrawData->TotalVtxCount;
	DisplayPos = DrawData->DisplayPos;
	DisplaySize = DrawData->DisplaySize;
	FramebufferScale = DrawData->FramebufferScale;
	OwnerViewport = DrawData->OwnerViewport;

	CmdLists.Empty();
	for(int i=0; i<DrawData->CmdListsCount; ++i)
	{
		FImGuiDrawList& Cmd = CmdLists.Add_GetRef({});
		Cmd.Set(DrawData->CmdLists[i]);
	}
}

class SImGuiCanvas : public SLeafWidget
{
	SLATE_DECLARE_WIDGET(SImGuiCanvas, SLeafWidget)

public:
	SLATE_BEGIN_ARGS(SImGuiCanvas)
		: _ViewportID(-1), _Identifier()
	{}

	SLATE_ATTRIBUTE(ImGuiID, ViewportID)
	SLATE_ATTRIBUTE(FName, Identifier)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

public:
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	void UpdateDrawData(ImDrawData* InDrawData);
	
private:
	ImGuiID ViewportID;
	FName Identifier;
	FImGuiDrawData DrawData = {};
	FVector2f CachedPosition;
	int DisableThrottling = 0;
};

SLATE_IMPLEMENT_WIDGET(SImGuiCanvas)

void SImGuiCanvas::PrivateRegisterAttributes(FSlateAttributeInitializer&)
{
}

void SImGuiCanvas::Construct(const FArguments& InArgs)
{
	ViewportID = InArgs._ViewportID.Get();
	Identifier = InArgs._Identifier.Get();

	SetVisibility(EVisibility::Visible);
}

int32 SImGuiCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	static const FSlateColorBrush SolidWhiteBrush = FSlateColorBrush( FColorList::White );

	// @NOTE: We only use the translation since we're outputting vertices in local space with scaling ignored
	// we also need to offset by DisplayPos since the vertices are in Desktop space
	FSlateRenderTransform GeoRenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();
	GeoRenderTransform = GeoRenderTransform.GetTranslation() - FVector2D{DrawData.DisplayPos.x, DrawData.DisplayPos.y};
	
	for(int DrawListIdx=0; DrawListIdx<DrawData.CmdListsCount; ++DrawListIdx)
	{
		const FImGuiDrawList& DrawList = DrawData.CmdLists[DrawListIdx];
		
		TArray<FSlateVertex> VertexBuffer;
		VertexBuffer.SetNumUninitialized(DrawList.VtxBuffer.Num(), false);
		for(int VtxIndex = 0; VtxIndex < DrawList.VtxBuffer.Num(); ++VtxIndex)
		{
			const ImDrawVert& Vtx = DrawList.VtxBuffer[VtxIndex];
			ImVec4 Color = ImGui::ColorConvertU32ToFloat4(Vtx.col);
			FLinearColor LinearColor = {Color.x, Color.y, Color.z, Color.w};
			VertexBuffer[VtxIndex] = FSlateVertex::Make<ESlateVertexRounding::Enabled>(
				GeoRenderTransform,
				FVector2f{Vtx.pos.x, Vtx.pos.y},
				FVector2f{Vtx.uv.x, Vtx.uv.y},
				FVector2f{1.0f, 1.0f },
				LinearColor.ToFColor(false));
		}
		
		TArray<SlateIndex> IndexBuffer;
		for(int CmdIndex = 0; CmdIndex<DrawList.CmdBuffer.Num(); ++CmdIndex)
		{
			const ImDrawCmd& DrawCmd = DrawList.CmdBuffer[CmdIndex];

			FSlateBrush* Brush = (FSlateBrush*)DrawCmd.TextureId;
			const FSlateResourceHandle& Handle = Brush ? Brush->GetRenderingResource() : SolidWhiteBrush.GetRenderingResource();
			
			IndexBuffer.SetNumUninitialized(DrawCmd.ElemCount, false);
			for(int ElemIndex = 0; ElemIndex<static_cast<int>(DrawCmd.ElemCount); ++ElemIndex)
			{
				IndexBuffer[ElemIndex] = DrawList.IdxBuffer[DrawCmd.IdxOffset + ElemIndex];
			}

			FVector2f ClippingRectTopLeft = {DrawCmd.ClipRect.x, DrawCmd.ClipRect.y};
			ClippingRectTopLeft = GeoRenderTransform.TransformPoint(ClippingRectTopLeft);
			FVector2f ClippingRectBottomRight = {DrawCmd.ClipRect.z, DrawCmd.ClipRect.w};
			ClippingRectBottomRight = GeoRenderTransform.TransformPoint(ClippingRectBottomRight);
			FSlateRect ClippingRect = FSlateRect(
				ClippingRectTopLeft.X,
				ClippingRectTopLeft.Y,
				ClippingRectBottomRight.X,
				ClippingRectBottomRight.Y);
			
			ClippingRect = ClippingRect.IntersectionWith(MyCullingRect);
			OutDrawElements.PushClip(FSlateClippingZone{ ClippingRect });
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, VertexBuffer, IndexBuffer, nullptr, 0, 0);
			OutDrawElements.PopClip();
		}
	}
	
	return LayerId;
}

FVector2D SImGuiCanvas::ComputeDesiredSize(float) const
{
	return FVector2D{0};
}

bool SImGuiCanvas::SupportsKeyboardFocus() const
{
	return true;
}

FReply SImGuiCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = ImGui::GetIO();
	const FKey EffectingButton = MouseEvent.GetEffectingButton();
	if(EffectingButton == EKeys::LeftMouseButton)
	{
		IO.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
	}
	else if(EffectingButton == EKeys::RightMouseButton)
	{
		IO.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
	}
	else if(EffectingButton == EKeys::MiddleMouseButton)
    {
    	IO.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
    }

	if(IO.WantCaptureMouse)
	{
		FSlateThrottleManager::Get().DisableThrottle(true);
		DisableThrottling++;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SImGuiCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = ImGui::GetIO();
	const FKey EffectingButton = MouseEvent.GetEffectingButton();
	
	if(EffectingButton == EKeys::LeftMouseButton)
	{
		IO.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
	}
	else if(EffectingButton == EKeys::RightMouseButton)
	{
		IO.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
	}
	else if(EffectingButton == EKeys::MiddleMouseButton)
	{
		IO.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
	}
	
	if(HasMouseCapture())
	{
		while(DisableThrottling)
		{
			FSlateThrottleManager::Get().DisableThrottle(false);
			DisableThrottling--;
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SImGuiCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = ImGui::GetIO();
	FVector2f Position = MouseEvent.GetScreenSpacePosition();
	IO.AddMousePosEvent(Position.X, Position.Y);

	CachedPosition = Position;
	if(IO.WantCaptureMouse)
	{
	}
	return FReply::Handled();
}

void SImGuiCanvas::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = ImGui::GetIO();
	IO.AddMouseViewportEvent(ViewportID);
}

void SImGuiCanvas::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = ImGui::GetIO();
	IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

void SImGuiCanvas::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}

FReply SImGuiCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ImGuiIO& IO = ImGui::GetIO();

	IO.AddMouseWheelEvent(MouseEvent.GetGestureDelta().X, MouseEvent.GetWheelDelta());
	if(IO.WantCaptureMouse)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SImGuiCanvas::UpdateDrawData(ImDrawData* InDrawData)
{
	DrawData.Set(InDrawData);
}

void UImGuiSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

struct ImGui_ImplUnreal_ViewportData
{
	SWindow* HwndParent = nullptr;
	TSharedPtr<SWindow> Hwnd = nullptr;
	TSharedPtr<SImGuiCanvas> Canvas = nullptr;

	bool bHwndOwned = false;
};

static SWindow* ImGui_ImplUnreal_GetHwndFromViewportID(ImGuiID ID)
{
	if(ID != 0)
	{
		if (ImGuiViewport* Viewport = ImGui::FindViewportByID(ID))
		{
			return static_cast<SWindow*>(Viewport->PlatformHandle); 
		}
	}
	
	return nullptr;
}

static void ImGui_ImplUnreal_UpdateMonitors()
{
	FDisplayMetrics DisplayMetrics;
	
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	ImGuiPlatformIO& io = ImGui::GetPlatformIO();
	for(const FMonitorInfo& MonitorInfo : DisplayMetrics.MonitorInfo)
	{
		ImGuiPlatformMonitor ImguiMonitor;
		ImguiMonitor.MainPos = ImVec2((float)MonitorInfo.DisplayRect.Left, (float)MonitorInfo.DisplayRect.Top);
		ImguiMonitor.MainSize = ImVec2((float)(MonitorInfo.DisplayRect.Right - MonitorInfo.DisplayRect.Left), (float)(MonitorInfo.DisplayRect.Bottom - MonitorInfo.DisplayRect.Top));
		ImguiMonitor.WorkPos = ImVec2((float)MonitorInfo.WorkArea.Left, (float)MonitorInfo.WorkArea.Top);
		ImguiMonitor.WorkSize = ImVec2((float)(MonitorInfo.WorkArea.Right - MonitorInfo.WorkArea.Left), (float)(MonitorInfo.WorkArea.Bottom - MonitorInfo.WorkArea.Top));
		ImguiMonitor.DpiScale = MonitorInfo.DPI / 96.0f; // DpiScale is 1.0f == 96dpi
		ImguiMonitor.PlatformHandle = nullptr; // @NOTE: No platform handle access in unreal, might not be needed

		if(MonitorInfo.bIsPrimary)
		{
			io.Monitors.push_front(ImguiMonitor);
		}
		else
		{
			io.Monitors.push_back(ImguiMonitor);
		}
	}
}

static void ImGui_ImplUnreal_CreateWindow(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* VD = IM_NEW(ImGui_ImplUnreal_ViewportData)();
	Viewport->PlatformUserData = VD;

	// Select style and parent window
	VD->HwndParent = ImGui_ImplUnreal_GetHwndFromViewportID(Viewport->ParentViewportId);

	// Create window
	SAssignNew(VD->Hwnd, SWindow)
					.UseOSWindowBorder(false)
					.HasCloseButton(false)
					.CreateTitleBar(false)
					.LayoutBorder({0})
					.SizingRule(ESizingRule::FixedSize)
					.IsTopmostWindow(true)
					.FocusWhenFirstShown(false)
	.Content()
	[
		SAssignNew(VD->Canvas, SImGuiCanvas)
		.Identifier("ImGuiWindow")
		.ViewportID(Viewport->ID)
	];
	VD->bHwndOwned = true;
	
	FSlateRect WindowPosition;
	WindowPosition.Left = Viewport->Pos.x;
	WindowPosition.Top = Viewport->Pos.y;
	WindowPosition.Right = WindowPosition.Left + Viewport->Size.x;
	WindowPosition.Bottom = WindowPosition.Top + Viewport->Size.y;
	VD->Hwnd->ReshapeWindow(WindowPosition);

	if(VD->HwndParent) // @TODO: This might be wrong, no idea what ParentViewportId represents exactly for now
	{
		VD->HwndParent->AddChildWindow(VD->Hwnd.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(VD->Hwnd.ToSharedRef(), false);
	}

	Viewport->PlatformRequestResize = false;
	Viewport->PlatformHandle = VD->Hwnd.Get();
}

static void ImGui_ImplUnreal_DestroyWindow(ImGuiViewport* Viewport)
{
	if(ImGui_ImplUnreal_ViewportData* vd = static_cast<ImGui_ImplUnreal_ViewportData*>(Viewport->PlatformUserData))
	{
		// @NOTE: Transfer capture here, need to figure out how to check if a child of an SWindow has capture (HasMouseCapture might be sufficient)
		if(vd->Hwnd->HasMouseCapture() || vd->Canvas->HasMouseCapture())
		{
			FSlateApplication::Get().ReleaseAllPointerCapture();
			// @TODO: Give capture to main window
		}
		
		if(vd->Hwnd && vd->bHwndOwned)
		{
			vd->Hwnd->RequestDestroyWindow();
		}
		vd->Hwnd = nullptr;
		vd->Canvas = nullptr;
		IM_DELETE(vd);
	}

	Viewport->PlatformUserData = Viewport->PlatformHandle = nullptr;
}

static void ImGui_ImplUnreal_ShowWindow(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* vd = static_cast<ImGui_ImplUnreal_ViewportData*>(Viewport->PlatformUserData);
	check(vd->Hwnd);
	vd->Hwnd->ShowWindow();
	if(!(Viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing))
	{
		vd->Hwnd->GetNativeWindow()->SetWindowFocus();
	}
}

static void ImGui_ImplUnreal_UpdateWindow(ImGuiViewport* Viewport)
{
	// @TODO
}

static void ImGui_ImplUnreal_SetWindowPos(ImGuiViewport* Viewport, ImVec2 ImVec2)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	check(vd->Hwnd);

	const FVector2f NewPosition = { ImVec2.x, ImVec2.y };
	vd->Hwnd->MoveWindowTo(NewPosition);
}

static ImVec2 ImGui_ImplUnreal_GetWindowPos(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	if(!vd->Canvas)
	{
		return ImVec2{0, 0};
	}

	// Viewport->PlatformRequestMove = true;
	FGeometry CachedGeometry = vd->Canvas->GetCachedGeometry();
	FVector2f Pos = CachedGeometry.GetAbsolutePosition();
	ImVec2 ImPos;
	ImPos.x = Pos.X;
	ImPos.y = Pos.Y;

	// UE_LOG(LogTemp, Warning, TEXT("Position: %.2f %.2f"), ImPos.x, ImPos.y);
	
	return ImPos;
}

static ImVec2 ImGui_ImplUnreal_GetWindowSize(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	check(vd->Hwnd);
	
	const FVector2f Size = vd->Hwnd->GetSizeInScreen();

	const ImVec2 ImSize = { Size.X, Size.Y };
	return ImSize;
}

static void ImGui_ImplUnreal_SetWindowSize(ImGuiViewport* Viewport, ImVec2 ImVec2)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	check(vd->Hwnd);
	
	vd->Hwnd->Resize(FVector2f{ ImVec2.x, ImVec2.y });
}

static void ImGui_ImplUnreal_SetWindowFocus(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	check(vd->Hwnd);

	vd->Hwnd->BringToFront();
	// vd->Hwnd->HACK_ForceToFront(); // ::SetForegroundWindow
	vd->Hwnd->GetNativeWindow()->SetWindowFocus();
}

static bool ImGui_ImplUnreal_GetWindowFocus(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	if(vd->Hwnd)
	{
		return vd->Hwnd->HasAnyUserFocusOrFocusedDescendants();
	}
	return false;
}

static bool ImGui_ImplUnreal_GetWindowMinimized(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	if(vd->Hwnd)
	{
		return vd->Hwnd->IsWindowMinimized();
	}
	return false;
}

static void ImGui_ImplUnreal_SetWindowTitle(ImGuiViewport* Viewport, const char* Arg)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	check(vd->Hwnd);

	vd->Hwnd->SetTitle(FText::FromString(Arg));
}

static void ImGui_ImplUnreal_SetWindowAlpha(ImGuiViewport* Viewport, float Alpha)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	check(vd->Hwnd);
	check(Alpha >= 0.0f && Alpha <= 1.0f);
	
	vd->Hwnd->SetOpacity(Alpha);
}

static float ImGui_ImplUnreal_GetWindowDpiScale(ImGuiViewport* Viewport)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	if(vd->Hwnd)
		return vd->Hwnd->GetDPIScaleFactor();

	return 1.0f;
}

static void ImGui_ImplUnreal_OnChangedViewport(ImGuiViewport* Viewport)
{
	// @NOTE: unimplemented
}

static void ImGui_ImplUnreal_RenderWindow(ImGuiViewport* Viewport, void*)
{
	ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)Viewport->PlatformUserData;
	if(vd->Canvas)
	{
		vd->Canvas->UpdateDrawData(Viewport->DrawData);
	}
}

static ImGuiStyle GetImGuiStyle()
{
	ImGuiStyle Style = ImGuiStyle();
	Style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	Style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	Style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	Style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	Style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	Style.Colors[ImGuiCol_Border]                = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	Style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	Style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	Style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	Style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	Style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	Style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	Style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	Style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	Style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	Style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	Style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	Style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	Style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	Style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	Style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
	Style.Colors[ImGuiCol_Button]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	Style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	Style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	Style.Colors[ImGuiCol_Header]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	Style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	Style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	Style.Colors[ImGuiCol_Separator]             = Style.Colors[ImGuiCol_Border];
	Style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
	Style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	Style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	Style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
	Style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	Style.Colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.08f, 0.09f, 0.83f);
	Style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.33f, 0.34f, 0.36f, 0.83f);
	Style.Colors[ImGuiCol_TabActive]             = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
	Style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	Style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	Style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	Style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	Style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	Style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	Style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	Style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	Style.Colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	Style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	Style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	Style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	Style.ChildRounding = 4.0f;
	Style.FrameRounding = 4.0f;
	Style.GrabRounding = 4.0f;
	Style.PopupRounding = 4.0f;
	Style.ScrollbarRounding = 4.0f;
	Style.TabRounding = 4.0f;
	Style.WindowRounding = 4.0f;
	return Style;
}

void UImGuiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	check(!ImGui::GetCurrentContext()); // init imgui only once
	ImGui::CreateContext();
	
	ImGuiIO& IO = ImGui::GetIO();

	unsigned char* ImPixels;
	int FontAtlasWidth, FontAtlasHeight, FontAtlasBPP;
	// Setup font texture
	IO.Fonts->GetTexDataAsRGBA32(&ImPixels, &FontAtlasWidth, &FontAtlasHeight, &FontAtlasBPP);
	check(FontAtlasBPP == 4);
	
	FontTexture = UTexture2D::CreateTransient(FontAtlasWidth, FontAtlasHeight, PF_R8G8B8A8, "ImGuiDefaultFontTexture");

	FontTexture->SRGB = false;
	FontTexture->CompressionNone = true;
	FontTexture->MipGenSettings = TMGS_NoMipmaps;
	FontTexture->CompressionSettings = TC_Default;
	FontTexture->bNotOfflineProcessed = true;
	
	uint8* MipData = static_cast<uint8*>(FontTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	FMemory::Memcpy(MipData, ImPixels, FontTexture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize());
	FontTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	FontTexture->UpdateResource();
	
	FontTextureBrush = MakeUnique<FSlateImageBrush>(FontTexture, FVector2f{static_cast<float>(FontAtlasWidth), static_cast<float>(FontAtlasHeight)});
	IO.Fonts->SetTexID((ImTextureID)FontTextureBrush.Get());
	
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	const FString SavedDir = FPaths::ProjectSavedDir();
	FString Directory = FPaths::Combine(*SavedDir, TEXT("ImGui"));
	// Make sure that directory is created.
	IPlatformFile::GetPlatformPhysical().CreateDirectory(*Directory);

	IniFileName = TCHAR_TO_ANSI(*FPaths::Combine(Directory, "PIEContext0.ini")); 
	IO.IniFilename = IniFileName.c_str();
	
	ImGui::GetStyle() = GetImGuiStyle();

	ImGuiStyle& style = ImGui::GetStyle();
	if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	IO.BackendPlatformName = "Unreal";
	IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
	IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
	IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
	IO.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can call io.AddMouseViewportEvent() with correct data (optional)
	IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;	  // Backend Renderer supports multiple viewports.

	// Platform setup
	ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();

	ImGui_ImplUnreal_UpdateMonitors();
	
	PlatformIO.Platform_CreateWindow = ImGui_ImplUnreal_CreateWindow;
	PlatformIO.Platform_DestroyWindow = ImGui_ImplUnreal_DestroyWindow;
	PlatformIO.Platform_ShowWindow = ImGui_ImplUnreal_ShowWindow;
	PlatformIO.Platform_SetWindowPos = ImGui_ImplUnreal_SetWindowPos;
	PlatformIO.Platform_GetWindowPos = ImGui_ImplUnreal_GetWindowPos;
	PlatformIO.Platform_SetWindowSize = ImGui_ImplUnreal_SetWindowSize;
	PlatformIO.Platform_GetWindowSize = ImGui_ImplUnreal_GetWindowSize;
	PlatformIO.Platform_SetWindowFocus = ImGui_ImplUnreal_SetWindowFocus;
	PlatformIO.Platform_GetWindowFocus = ImGui_ImplUnreal_GetWindowFocus;
	PlatformIO.Platform_GetWindowMinimized = ImGui_ImplUnreal_GetWindowMinimized;
	PlatformIO.Platform_SetWindowTitle = ImGui_ImplUnreal_SetWindowTitle;
	PlatformIO.Platform_SetWindowAlpha = ImGui_ImplUnreal_SetWindowAlpha;
	PlatformIO.Platform_UpdateWindow = ImGui_ImplUnreal_UpdateWindow;
	PlatformIO.Platform_GetWindowDpiScale = ImGui_ImplUnreal_GetWindowDpiScale; // FIXME-DPI
	PlatformIO.Platform_OnChangedViewport = ImGui_ImplUnreal_OnChangedViewport; // FIXME-DPI

	PlatformIO.Renderer_RenderWindow = ImGui_ImplUnreal_RenderWindow;

	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui_ImplUnreal_ViewportData* vd = IM_NEW(ImGui_ImplUnreal_ViewportData)();
	vd->bHwndOwned = false;
	MainViewport->PlatformUserData = vd;
	
	FSlateApplication::Get().OnPreTick().AddUObject(this, &UImGuiSubsystem::EndFrame);
	FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &UImGuiSubsystem::PreWorldInitialization);
	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UImGuiSubsystem::WorldInitializedActors);
}



void UImGuiSubsystem::WindowTest()
{
	FSlateApplication::Get().AddWindow(SNew(SWindow).SizingRule(ESizingRule::FixedSize).ClientSize({200.0f, 200.0f}).Title(INVTEXT("Test Window 1")).Content()[SNew(SImGuiCanvas).Identifier("Test 1")]);
	FSlateApplication::Get().AddWindow(SNew(SWindow).SizingRule(ESizingRule::FixedSize).ClientSize({200.0f, 200.0f}).Title(INVTEXT("Test Window 2")).Content()[SNew(SImGuiCanvas).Identifier("Test 2")]);
}

void UImGuiSubsystem::PreWorldInitialization(UWorld* World, FWorldInitializationValues WorldInitializationValues)
{
	if(!bInImGuiFrame)
	{
		BeginFrame(0.016f);
	}
}

void UImGuiSubsystem::WorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams)
{
	if(!bInImGuiFrame)
	{
		EndFrame(0.016f);
	}
}

void UImGuiSubsystem::BeginFrame(float DeltaTime)
{
}

static TSharedPtr<SWindow> FindWindow(const TSharedPtr<SImGuiCanvas>& Canvas)
{
	TSharedPtr<SWidget> It = Canvas->GetParentWidget();
	while(It)
	{
		if(It->Advanced_IsWindow())
		{
			return StaticCastSharedPtr<SWindow>(It);
		}
		It = It->GetParentWidget();
	}

	return nullptr;
}

void UImGuiSubsystem::EndFrame(float DeltaTime)
{
	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if(bInImGuiFrame)
	{
		// UE_LOG(LogTemp, Warning, TEXT("=== ImGui Render ==="));
		ImGui::Render();

		ImGui_ImplUnreal_RenderWindow(ImGui::GetMainViewport(), nullptr);
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		bInImGuiFrame = false;
	}
	
	if(MainViewport->PlatformUserData && !GEngine->GameViewport) // Cleanup MainViewport data if the game viewport is suddenly invalid
	{
		ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)MainViewport->PlatformUserData;
		vd->Canvas = nullptr;
		vd->Hwnd = nullptr;
	}

	if(ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)MainViewport->PlatformUserData)
	{
		if(GEngine->GameViewport && !vd->Canvas) // Create the canvas if the GameViewport is valid
		{
			TSharedPtr<SImGuiCanvas> ImGuiCanvas;
			GEngine->GameViewport->AddViewportWidgetContent(SAssignNew(ImGuiCanvas, SImGuiCanvas)
				.Identifier("Main")
				.ViewportID(MainViewport->ID), 1000);

			// FSlateApplication::Get().
			vd->Hwnd = FindWindow(ImGuiCanvas);
			vd->bHwndOwned = false;
			vd->Canvas = ImGuiCanvas;
		}
	}

	if(!bInImGuiFrame)
	{
		ImGuiIO& IO = ImGui::GetIO();
		IO.DeltaTime = DeltaTime;

		if(ImGui_ImplUnreal_ViewportData* vd = (ImGui_ImplUnreal_ViewportData*)MainViewport->PlatformUserData; vd && vd->Canvas)
		{
			FVector2D ViewportSize;
			GEngine->GameViewport->GetViewportSize(ViewportSize);
			IO.DisplaySize = ImVec2{
				(float)ViewportSize.X,
				(float)ViewportSize.Y,
			};
		}
		else
		{
			IO.DisplaySize = ImVec2{
				0.0f,
				0.0f,
			};
		}

		if(FSlateApplication::Get().GetPlatformCursor()->GetType() == EMouseCursor::Type::None)
		{
			for(int i=0; i<ImGuiMouseButton_COUNT; ++i)
			{
				IO.AddMouseButtonEvent(i, false);
			}
			IO.AddMousePosEvent(-FLT_MIN, -FLT_MIN);
		}
		
		ImGui::NewFrame();
		bInImGuiFrame = true;

		/* @NOTE: Demo code to demonstrate editor time editing
		ImGui::ShowDemoWindow();

		if(GEditor)
		{
			if(ImGui::Begin("Test Editor Window"))
			{
				USelection* SelectedActors = GEditor->GetSelectedActors();
				if(!SelectedActors)
				{
					ImGui::Text("Nothing selected.");
				}
				else
				{
					if(AActor* TopObject = SelectedActors->GetTop<AActor>())
					{
						ImGui::Text("Selected %ls", *TopObject->GetName());
						ImGui::Text("Components: ");
						TArray<UActorComponent*> Components;
						TopObject->GetComponents<UActorComponent>(Components);
						for(auto& Component: Components)
						{
							auto NameANSI = StringCast<ANSICHAR>(*Component->GetName());
							FString Label = "Delete " + Component->GetName();
							if(ImGui::Button(TCHAR_TO_ANSI(*Label)))
							{
								Component->DestroyComponent();
							}	
						}
					}
					else
					{
						ImGui::Text("Nothing selected.");
					}
				}
			}
			ImGui::End();
		}
		*/
	}
}

void UImGuiSubsystem::LevelViewportClientListChanged() const
{
}

UE_ENABLE_OPTIMIZATION
