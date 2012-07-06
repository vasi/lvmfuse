#include <iostream>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>

int main() {
	CFMutableDictionaryRef match = IOServiceMatching("IOStorage");
	io_iterator_t iter;
	kern_return_t err = IOServiceGetMatchingServices(kIOMasterPortDefault,
		match, &iter);
	if (err != KERN_SUCCESS)
		return -1;
	io_registry_entry_t entry;
	while ((entry = IOIteratorNext(iter))) {
		CFStringRef bsdName = (CFStringRef)IORegistryEntryCreateCFProperty(
			entry, CFSTR(kIOBSDNameKey), NULL, 0);
		if (bsdName != NULL) {
			CFStringRef dev = CFStringCreateWithFormat(NULL, 0,
				CFSTR("/dev/%@"), bsdName);
			CFRelease(bsdName);
			size_t size = CFStringGetMaximumSizeOfFileSystemRepresentation(dev);
			std::vector<char> chars(size);
			CFStringGetFileSystemRepresentation(dev, &chars[0], size);
			CFRelease(dev);
			std::string s(&chars[0]);
			std::cout << s << "\n";
		}
		IOObjectRelease(entry);
	}
	IOObjectRelease(iter);
	
	
	return 0;
}
