- Change device tree is needed
```
&i2c1 {
	pinctrl-names = "default";
	pinctrl-0 = <&i2c1_pins>;
	clock-frequency = <100000>;
	status = "okay";

    my_bh1750: bh1750@23 {
        compatible = "willtek,bh1750";  /* 드라이버 소스의 compatible과 일치해야 함 */
        reg = <0x23>;                  /* I2C 주소 */
        status = "okay";
    };
};
```

