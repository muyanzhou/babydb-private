import matplotlib.pyplot as plt

# 数据准备
categories = ['Category 1', 'Category 2', 'Category 3', 'Category 4']
values = [9683, 49872, 25822, 12215]  # 单位ms
colors = ['skyblue', 'salmon', 'lightgreen', 'gold']

# 创建柱状图
plt.figure(figsize=(10, 6))
bars = plt.bar(categories, values, color=colors)

# 添加数值标签
for bar in bars:
    height = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2., height,
             f'{height}ms',
             ha='center', va='bottom')

# 添加标题和标签
plt.title('Performance Comparison (ms)', fontsize=15)
plt.xlabel('Categories', fontsize=12)
plt.ylabel('Time (milliseconds)', fontsize=12)

# 调整y轴范围使图表更美观
plt.ylim(0, max(values) * 1.1)

# 显示网格线
plt.grid(axis='y', linestyle='--', alpha=0.7)

# 显示图表
plt.tight_layout()
plt.show()