#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x1fdc7df2, "_mcount" },
	{ 0x92997ed8, "_printk" },
	{ 0x48b88ec8, "__platform_driver_register" },
	{ 0xc1514a3b, "free_irq" },
	{ 0xf79c9a6a, "gpiod_put" },
	{ 0xecb4ce45, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xe461822e, "gpiod_set_value" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0xfcec0987, "enable_irq" },
	{ 0x3ce4ca6f, "disable_irq" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0xc0cc44f1, "cdev_alloc" },
	{ 0x149fbba, "cdev_add" },
	{ 0x835b006b, "platform_get_irq" },
	{ 0xebfb39cd, "device_property_read_u32_array" },
	{ 0x5c7f9668, "gpiod_get_index" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0xd187a121, "platform_driver_unregister" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0xdcb764ad, "memset" },
	{ 0x1748a027, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmy-pdev");
MODULE_ALIAS("of:N*T*Cmy-pdevC*");

MODULE_INFO(srcversion, "B14B06040FFC2C889EAB329");
