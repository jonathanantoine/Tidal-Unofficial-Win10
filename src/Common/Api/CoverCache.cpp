#include "pch.h"
#include "CoverCache.h"
#include "../tools/AsyncHelpers.h"
#include <mutex>
#include "ImageUriResolver.h"
#include "../tools/TimeUtils.h"
std::mutex g_CoverFolderMutex;
std::once_flag g_CoverFolderOnceFlag;
concurrency::task<Windows::Storage::StorageFolder^> g_CoverFolderOnceTask;

concurrency::task<Windows::Storage::StorageFolder^> ensureCoverFolderAsync() {
	std::lock_guard < std::mutex > lg(g_CoverFolderMutex);
	std::call_once(g_CoverFolderOnceFlag, []() {
		g_CoverFolderOnceTask = concurrency::create_task( Windows::Storage::ApplicationData::Current->LocalFolder->CreateFolderAsync(L"covers", Windows::Storage::CreationCollisionOption::OpenIfExists));
	});
	return g_CoverFolderOnceTask;
}
Concurrency::task<Windows::Storage::Streams::IBuffer^> DownloadAsync(Platform::String^ url, concurrency::cancellation_token cancelToken) {
	auto client = ref new Windows::Web::Http::HttpClient();
	auto response = await concurrency::create_task(client->GetAsync(ref new Windows::Foundation::Uri(url)), cancelToken);
	if (response->StatusCode == Windows::Web::Http::HttpStatusCode::NotFound) {
		return (Windows::Storage::Streams::IBuffer^)nullptr;
	}
	response->EnsureSuccessStatusCode();
	return await concurrency::create_task(response->Content->ReadAsBufferAsync(), cancelToken);
}

concurrency::task<Platform::String^> api::EnsureCoverInCacheAsync(std::int64_t id, Platform::String ^ imageId, concurrency::cancellation_token cancelToken)
{
	auto coversFolder = await ensureCoverFolderAsync();
	auto existing = await concurrency::create_task(coversFolder->TryGetItemAsync(id.ToString() + L".jpg"));
	if (existing) {
		return ref new Platform::String( L"ms-appdata:///local/covers/") + id.ToString() + L".jpg";
	}
	while(!cancelToken.is_canceled()){
		try {
			if (imageId == nullptr || imageId->Length() == 0) {
				return await EnsureCoverInCacheAsync(0, L"0dfd3368-3aa1-49a3-935f-10ffb39803c0", cancelToken);
			}
			auto buffer = await DownloadAsync(resolveImageUri(imageId->Data(), 320, 320), cancelToken);
			if (!buffer) {
				return await EnsureCoverInCacheAsync(0, L"0dfd3368-3aa1-49a3-935f-10ffb39803c0", cancelToken);
			}
			
			auto file = await concurrency::create_task(coversFolder->CreateFileAsync(id.ToString() + L".jpg", Windows::Storage::CreationCollisionOption::ReplaceExisting));
			await concurrency::create_task(Windows::Storage::FileIO::WriteBufferAsync(file, buffer));
			return ref new Platform::String(L"ms-appdata:///local/covers/") + id.ToString() + L".jpg";
		}
		catch (...) {

		}
		// something went wrong
		await tools::async::WaitFor(tools::time::ToWindowsTimeSpan(std::chrono::seconds(1)), cancelToken);
	}
	concurrency::cancel_current_task();
}

concurrency::task<Platform::String^> api::EnsureCoverInCacheAsync(std::int64_t id, Platform::String ^ imageId, int width, int height, concurrency::cancellation_token cancelToken)
{
	auto fileName = id.ToString() + L"." + width.ToString() + L"x" + height.ToString() + L".jpg";
	auto coversFolder = await ensureCoverFolderAsync();
	auto existing = await concurrency::create_task(coversFolder->TryGetItemAsync(fileName));
	if (existing) {
		return ref new Platform::String(L"ms-appdata:///local/covers/") + fileName;
	}
	while (!cancelToken.is_canceled()) {
		try {
			if (imageId == nullptr || imageId->Length() == 0) {
				return await EnsureCoverInCacheAsync(0, L"0dfd3368-3aa1-49a3-935f-10ffb39803c0", width, height, cancelToken);
			}
			auto buffer = await DownloadAsync(resolveImageUri(imageId->Data(), width, height), cancelToken);
			if (!buffer) {
				return await EnsureCoverInCacheAsync(0, L"0dfd3368-3aa1-49a3-935f-10ffb39803c0", width, height, cancelToken);
			}

			auto file = await concurrency::create_task(coversFolder->CreateFileAsync(fileName, Windows::Storage::CreationCollisionOption::ReplaceExisting));
			await concurrency::create_task(Windows::Storage::FileIO::WriteBufferAsync(file, buffer));
			return ref new Platform::String(L"ms-appdata:///local/covers/") + fileName;
		}
		catch (...) {

		}
		// something went wrong
		await tools::async::WaitFor(tools::time::ToWindowsTimeSpan(std::chrono::seconds(1)), cancelToken);
	}
	concurrency::cancel_current_task();
}

concurrency::task<Platform::String^> api::GetCoverUriAndFallbackToWebAsync(std::int64_t id, Platform::String ^ imageId, int width, int height, concurrency::cancellation_token cancelToken)
{
	auto fileName = id.ToString() + L"." + width.ToString() + L"x" + height.ToString() + L".jpg";
	auto coversFolder = await ensureCoverFolderAsync();
	auto existing = await concurrency::create_task(coversFolder->TryGetItemAsync(fileName));
	if (existing) {
		return ref new Platform::String(L"ms-appdata:///local/covers/") + fileName;
	}
	return resolveImageUri(imageId->Data(), width, height);
}

Platform::String ^ api::getOfflineCoverUrl(std::int64_t id, int width, int height)
{
	auto fileName = id.ToString() + L"." + width.ToString() + L"x" + height.ToString() + L".jpg";
	return ref new Platform::String(L"ms-appdata:///local/covers/") + fileName;
}
